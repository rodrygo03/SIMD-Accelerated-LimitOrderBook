#!/bin/bash
# Single Configuration Benchmark Runner for SIMD-LOB Engine
# Usage: ./run_single_benchmark.sh <config> [events] [nasdaq_file]
# Examples: 
#   ./run_single_benchmark.sh fully-optimized                        # Uses default events & file
#   ./run_single_benchmark.sh fully-optimized 1000                   # Uses 1000 events & default file
#   ./run_single_benchmark.sh fully-optimized 1000 01302019          # Uses 1000 events & specific file
# For hardware metrics: sudo sysctl kernel.perf_event_paranoid=1
# For system cache clearing: export CLEAR_SYSTEM_CACHES=true

set -e

# Configuration
CLEAR_SYSTEM_CACHES=${CLEAR_SYSTEM_CACHES:-false}

# Function to clear system-level caches
clear_system_caches() {
    if [ "$CLEAR_SYSTEM_CACHES" = "true" ]; then
        echo "Clearing system caches (requires sudo)..."
        
        # Check if we have sudo access
        if ! sudo -n true 2>/dev/null; then
            echo "Warning: sudo access required for system cache clearing. Skipping system cache flush."
            echo "Run with: sudo CLEAR_SYSTEM_CACHES=true ./run_single_benchmark.sh $1 $2"
            return
        fi
        
        # Clear page cache, dentries, and inodes
        echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
        
        # Force memory compaction
        if [ -f /proc/sys/vm/compact_memory ]; then
            echo 1 | sudo tee /proc/sys/vm/compact_memory > /dev/null
        fi
        
        # Brief settling time
        sleep 1
        echo "System caches cleared"
    fi
}

# Check arguments - events and nasdaq_file parameters are optional
if [ $# -lt 1 ] || [ $# -gt 3 ]; then
    echo "Usage: $0 <config> [events] [nasdaq_file]"
    echo ""
    echo "Available configurations:"
    echo "  scalar-baseline      - Pure STL baseline (no optimizations)"
    echo "  simd-baseline       - SIMD optimizations only"
    echo "  object-pool-only    - Object pooling only"
    echo "  object-pool-simd    - Object pooling + SIMD (best combination)"
    echo "  cache-only          - Cache optimizations only"
    echo "  memory-optimized    - Memory optimizations (no SIMD)"
    echo "  fully-optimized     - All optimizations enabled"
    echo ""
    echo "Available NASDAQ files:"
    echo "  01302019, 03272019, 07302019, 08302019, 10302019, 12302019"
    echo "  (defaults to 01302019 if not specified)"
    echo ""
    echo "Examples:"
    echo "  $0 scalar-baseline                    # Uses default events & 01302019 file"
    echo "  $0 scalar-baseline 1000               # Uses 1000 events & 01302019 file"
    echo "  $0 scalar-baseline 1000 03272019      # Uses 1000 events & 03272019 file"
    echo "  $0 fully-optimized \"\" 07302019        # Uses default events & 07302019 file"
    exit 1
fi

CONFIG="$1"
EVENTS="${2:-96000}"  # Default for multi-file benchmarks
NASDAQ_FILE="${3:-01302019}"  # Default to January 30, 2019 file

echo "SIMD-LOB Single Configuration Benchmark"
echo "======================================="
echo "Configuration: $CONFIG"
echo "NASDAQ file: $NASDAQ_FILE"
if [ "$#" -eq 1 ]; then
    echo "Events: $EVENTS (using default)"
elif [ "$#" -eq 2 ] || ([ "$#" -eq 3 ] && [ -z "$2" ]); then
    echo "Events: $EVENTS"
else
    echo "Events: $EVENTS"
fi

# Check if we're in the right directory
if [ ! -f "../CMakePresets.json" ]; then
    echo "Error: This script must be run from the benchmarks directory"
    echo "Usage: cd benchmarks && ./run_single_benchmark.sh <config> <events>"
    exit 1
fi

# Change to root directory for build operations
cd ..

# Validate configuration
VALID_CONFIGS=("scalar-baseline" "simd-baseline" "object-pool-only" "object-pool-simd" "cache-only" "memory-optimized" "fully-optimized")
if [[ ! " ${VALID_CONFIGS[@]} " =~ " ${CONFIG} " ]]; then
    echo "Error: Invalid configuration '$CONFIG'"
    echo "Valid configurations: ${VALID_CONFIGS[@]}"
    exit 1
fi

# Handle empty events parameter (when user wants to specify nasdaq file but use default events)
if [ -z "$2" ] && [ -n "$3" ]; then
    EVENTS=96000
fi

# Validate events (must be positive integer, note: defaults to 96000 if not specified)
if ! [[ "$EVENTS" =~ ^[0-9]+$ ]] || [ "$EVENTS" -le 0 ]; then
    echo "Error: Events must be a positive integer, got '$EVENTS'"
    echo "Note: Events parameter defaults to 96000 if not specified"
    exit 1
fi

# Validate NASDAQ file
VALID_NASDAQ_FILES=("01302019" "03272019" "07302019" "08302019" "10302019" "12302019")
if [[ ! " ${VALID_NASDAQ_FILES[@]} " =~ " ${NASDAQ_FILE} " ]]; then
    echo "Error: Invalid NASDAQ file '$NASDAQ_FILE'"
    echo "Valid files: ${VALID_NASDAQ_FILES[@]}"
    exit 1
fi

# Set data file path based on selected NASDAQ file
ITCH_DATA_FILE="benchmark/data/${NASDAQ_FILE}.NASDAQ_ITCH50"
if [ ! -f "$ITCH_DATA_FILE" ]; then
    echo "Error: NASDAQ ITCH data file not found: $ITCH_DATA_FILE"
    echo "Please ensure the data file exists."
    echo "Available files:"
    ls -1 benchmark/data/*.NASDAQ_ITCH50 2>/dev/null || echo "  No .NASDAQ_ITCH50 files found"
    exit 1
fi

echo "Data file: $ITCH_DATA_FILE"
echo ""

# Build directory
BUILD_DIR="build-$CONFIG"

echo "Building $CONFIG configuration..."

# Configure with preset
cmake --preset="$CONFIG" > /dev/null 2>&1 || {
    echo "Error: Failed to configure preset '$CONFIG'"
    exit 1
}

# Build with limited parallelism
cmake --build --preset="$CONFIG" -j2 > /dev/null 2>&1 || {
    echo "Error: Failed to build configuration '$CONFIG'"
    exit 1
}

echo "✓ $CONFIG built successfully"

# Check if benchmark executable exists
if [ ! -f "$BUILD_DIR/benchmark/comprehensive_benchmark" ]; then
    echo "Error: Benchmark executable not found at $BUILD_DIR/benchmark/comprehensive_benchmark"
    exit 1
fi

echo ""
echo "Running benchmark..."

# Create results directory in benchmark folder with new naming format
RESULTS_DIR="benchmark/results"
mkdir -p "$RESULTS_DIR"

# Set environment variables
export ITCH_DATA_FILE="$(pwd)/$ITCH_DATA_FILE"
export BENCHMARK_SYMBOL=""
export RESULTS_DIR="$(pwd)/$RESULTS_DIR"
export MAX_EVENTS_PER_TEST="$EVENTS"
export BENCHMARK_CONFIG="$CONFIG"

# Clear system caches before benchmark
clear_system_caches

# Run benchmark
cd "$BUILD_DIR"
echo "Executing benchmark with $EVENTS events..."
./benchmark/comprehensive_benchmark || {
    echo "Error: Benchmark execution failed"
    cd ..
    exit 1
}
cd ..

echo "✓ Benchmark completed successfully"

# Show results
echo ""
echo "Results Summary:"
echo "==============="

# Expected result file with new naming format
EXPECTED_RESULT_FILE="$RESULTS_DIR/${CONFIG}_${EVENTS}_${NASDAQ_FILE}.NASDAQ_ITCH50.csv"

if [ -f "$EXPECTED_RESULT_FILE" ]; then
    echo "Results saved to: $EXPECTED_RESULT_FILE"
    echo ""
    echo "Performance Data:"
    cat "$EXPECTED_RESULT_FILE"
else
    echo "Expected result file not found: $EXPECTED_RESULT_FILE"
    echo "Checking for any result files in $RESULTS_DIR/:"
    ls -la "$RESULTS_DIR/" | grep -E "${CONFIG}_.*\.csv" || echo "  No result files found"
fi

echo ""
echo "Single benchmark completed!"
echo "Configuration: $CONFIG"
echo "Events processed: $EVENTS"
echo "NASDAQ file: $NASDAQ_FILE"
echo "Results location: $RESULTS_DIR/"