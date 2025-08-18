#!/bin/bash
# Comprehensive Benchmark Results Runner for SIMD-LOB Engine
# Runs all configurations across all NASDAQ files and generates visualization reports
# Usage: ./results.sh [events] [nasdaq_file|--all-files] [--skip-build] [--skip-viz] [--help]

set -e

# Default configuration
DEFAULT_EVENTS=96000
DEFAULT_NASDAQ_FILE="01302019"
ALL_NASDAQ_FILES=("01302019" "03272019" "07302019" "08302019" "10302019" "12302019")
CONFIGS=("scalar-baseline" "simd-baseline" "object-pool-only" "object-pool-simd" "cache-only" "memory-optimized" "fully-optimized")
EVENTS=""
NASDAQ_FILE=""
RUN_ALL_FILES=false
SKIP_BUILD=false
SKIP_VISUALIZATION=false
RESULTS_BASE_DIR="results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULTS_DIR="${RESULTS_BASE_DIR}/run_${TIMESTAMP}"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-viz|--skip-visualization)
            SKIP_VISUALIZATION=true
            shift
            ;;
        --all-files)
            RUN_ALL_FILES=true
            shift
            ;;
        --help|-h)
            echo "Comprehensive Benchmark Results Runner for SIMD-LOB Engine"
            echo ""
            echo "Usage: $0 [events] [nasdaq_file|--all-files] [options]"
            echo ""
            echo "Arguments:"
            echo "  events                Number of events to benchmark (default: $DEFAULT_EVENTS)"
            echo "  nasdaq_file           NASDAQ file to use (default: $DEFAULT_NASDAQ_FILE)"
            echo "  --all-files           Run all configurations across ALL NASDAQ files"
            echo ""
            echo "Available NASDAQ files:"
            echo "  01302019, 03272019, 07302019, 08302019, 10302019, 12302019"
            echo ""
            echo "Options:"
            echo "  --skip-build         Skip the build/compilation step"
            echo "  --skip-viz           Skip visualization generation"
            echo "  --help, -h           Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                          # Default events ($DEFAULT_EVENTS) & file ($DEFAULT_NASDAQ_FILE)"
            echo "  $0 8000                     # 8000 events & default file ($DEFAULT_NASDAQ_FILE)"
            echo "  $0 8000 03272019            # 8000 events & March 27, 2019 file"
            echo "  $0 --all-files              # All configs across ALL NASDAQ files"
            echo "  $0 8000 --all-files         # All configs across ALL files with 8000 events"
            echo "  $0 --skip-build             # Skip build, use defaults"
            echo "  $0 5000 01302019 --skip-viz # 5000 events, Jan 30 file, skip visualization"
            echo ""
            echo "Performance optimization settings:"
            echo "  - Sets kernel.perf_event_paranoid=1 for hardware metrics"
            echo "  - Enables system cache clearing between runs"
            echo "  - Automatically creates timestamped results directory"
            exit 0
            ;;
        [0-9]*)
            if [[ -z "$EVENTS" ]]; then
                EVENTS=$1
            else
                echo "Error: Multiple numeric arguments provided"
                exit 1
            fi
            shift
            ;;
        [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9])
            # NASDAQ file format: MMDDYYYY
            if [[ -z "$NASDAQ_FILE" ]]; then
                NASDAQ_FILE=$1
            else
                echo "Error: Multiple NASDAQ file arguments provided"
                exit 1
            fi
            shift
            ;;
        *)
            # Check if it's a potential NASDAQ file pattern
            if [[ $1 =~ ^[0-9]{8}$ ]]; then
                if [[ -z "$NASDAQ_FILE" ]]; then
                    NASDAQ_FILE=$1
                else
                    echo "Error: Multiple NASDAQ file arguments provided"
                    exit 1
                fi
            else
                echo "Error: Unknown option $1"
                echo "Use --help for usage information"
                exit 1
            fi
            shift
            ;;
    esac
done

# Set defaults if not specified
EVENTS=${EVENTS:-$DEFAULT_EVENTS}
if [ "$RUN_ALL_FILES" = false ]; then
    NASDAQ_FILE=${NASDAQ_FILE:-$DEFAULT_NASDAQ_FILE}
fi

# Validate events parameter
if ! [[ "$EVENTS" =~ ^[0-9]+$ ]] || [ "$EVENTS" -le 0 ]; then
    echo "Error: Events must be a positive integer, got '$EVENTS'"
    exit 1
fi

# Validate NASDAQ file (only if not running all files)
if [ "$RUN_ALL_FILES" = false ]; then
    VALID_NASDAQ_FILES=("01302019" "03272019" "07302019" "08302019" "10302019" "12302019")
    if [[ ! " ${VALID_NASDAQ_FILES[@]} " =~ " ${NASDAQ_FILE} " ]]; then
        echo "Error: Invalid NASDAQ file '$NASDAQ_FILE'"
        echo "Valid files: ${VALID_NASDAQ_FILES[@]}"
        exit 1
    fi
fi

echo "SIMD-LOB Comprehensive Benchmark Results Runner"
echo "==============================================="
echo "Events per benchmark: $EVENTS"
if [ "$RUN_ALL_FILES" = true ]; then
    echo "NASDAQ files: ALL (${ALL_NASDAQ_FILES[*]})"
    echo "Total combinations: $((${#CONFIGS[@]} * ${#ALL_NASDAQ_FILES[@]}))"
else
    echo "NASDAQ file: $NASDAQ_FILE"
    echo "Total combinations: ${#CONFIGS[@]}"
fi
echo "Results directory: $RESULTS_DIR"
echo "Skip build: $SKIP_BUILD"
echo "Skip visualization: $SKIP_VISUALIZATION"
echo "Timestamp: $TIMESTAMP"
echo ""

# Check if we're in the right directory
if [ ! -f "run_single_benchmark.sh" ]; then
    echo "Error: This script must be run from the benchmark directory"
    echo "Current directory: $(pwd)"
    echo "Expected files: run_single_benchmark.sh"
    exit 1
fi

# Check if ITCH data files exist
if [ "$RUN_ALL_FILES" = true ]; then
    echo "Checking availability of all NASDAQ files..."
    for nasdaq_file in "${ALL_NASDAQ_FILES[@]}"; do
        itch_file="data/${nasdaq_file}.NASDAQ_ITCH50"
        if [ ! -f "$itch_file" ]; then
            echo "Error: NASDAQ ITCH data file not found: $itch_file"
            echo "Please ensure all data files exist."
            echo "Available files:"
            ls -1 data/*.NASDAQ_ITCH50 2>/dev/null || echo "  No .NASDAQ_ITCH50 files found"
            exit 1
        else
            echo "  ✓ $itch_file"
        fi
    done
else
    ITCH_DATA_FILE="data/${NASDAQ_FILE}.NASDAQ_ITCH50"
    if [ ! -f "$ITCH_DATA_FILE" ]; then
        echo "Error: NASDAQ ITCH data file not found: $ITCH_DATA_FILE"
        echo "Please ensure the data file exists."
        echo "Available files:"
        ls -1 data/*.NASDAQ_ITCH50 2>/dev/null || echo "  No .NASDAQ_ITCH50 files found"
        exit 1
    fi
fi

# Create timestamped results directory
mkdir -p "$RESULTS_DIR"
echo "Created results directory: $RESULTS_DIR"

# Setup performance monitoring (requires sudo)
echo "Setting up performance monitoring..."
if sudo -n true 2>/dev/null; then
    sudo sysctl kernel.perf_event_paranoid=1
    echo "✓ Hardware performance counters enabled"
else
    echo "⚠ Warning: Cannot enable hardware performance counters (sudo required)"
    echo "  Hardware metrics will be limited or unavailable"
fi

# Enable system cache clearing
export CLEAR_SYSTEM_CACHES=true
echo "✓ System cache clearing enabled"


# Determine which NASDAQ files to run
if [ "$RUN_ALL_FILES" = true ]; then
    NASDAQ_FILES_TO_RUN=("${ALL_NASDAQ_FILES[@]}")
else
    NASDAQ_FILES_TO_RUN=("$NASDAQ_FILE")
fi

TOTAL_RUNS=$((${#CONFIGS[@]} * ${#NASDAQ_FILES_TO_RUN[@]}))

echo ""
if [ "$RUN_ALL_FILES" = true ]; then
    echo "Running benchmarks for ${#CONFIGS[@]} configurations across ${#NASDAQ_FILES_TO_RUN[@]} NASDAQ files..."
    echo "Total benchmark runs: $TOTAL_RUNS"
else
    echo "Running benchmarks for ${#CONFIGS[@]} configurations..."
fi
echo ""

# Track timing and results
start_time=$(date +%s)
failed_runs=()
successful_runs=()
run_count=0

# Run each configuration against each NASDAQ file
for nasdaq_file in "${NASDAQ_FILES_TO_RUN[@]}"; do
    echo ""
    echo "=== NASDAQ File: $nasdaq_file ==="
    echo ""
    
    for i in "${!CONFIGS[@]}"; do
        config="${CONFIGS[$i]}"
        run_count=$((run_count + 1))
        
        if [ "$RUN_ALL_FILES" = true ]; then
            echo "[$run_count/$TOTAL_RUNS] Running $config with $nasdaq_file"
        else
            echo "[$((i+1))/${#CONFIGS[@]}] Running configuration: $config"
        fi
        echo "=================================================="
        
        config_start=$(date +%s)
        
        if ./run_single_benchmark.sh "$config" "$EVENTS" "$nasdaq_file"; then
            config_end=$(date +%s)
            duration=$((config_end - config_start))
            echo "✓ $config with $nasdaq_file completed in ${duration}s"
            successful_runs+=("${config}_${nasdaq_file}")
            
            # Copy results to timestamped directory
            RESULT_FILE="results/${config}_${EVENTS}_${nasdaq_file}.NASDAQ_ITCH50.csv"
            if [ -f "$RESULT_FILE" ]; then
                cp "$RESULT_FILE" "$RESULTS_DIR/"
                echo "  Results copied to $RESULTS_DIR/"
            fi
        else
            echo "✗ $config with $nasdaq_file failed"
            failed_runs+=("${config}_${nasdaq_file}")
        fi
        
        echo ""
    done
done

# Summary of benchmark runs
end_time=$(date +%s)
total_duration=$((end_time - start_time))

echo "Benchmark Summary"
echo "================="
echo "Total time: ${total_duration}s"
echo "Total runs: $run_count"
echo "Successful runs: ${#successful_runs[@]}"
for run in "${successful_runs[@]}"; do
    echo "  ✓ $run"
done

if [ ${#failed_runs[@]} -gt 0 ]; then
    echo "Failed runs: ${#failed_runs[@]}"
    for run in "${failed_runs[@]}"; do
        echo "  ✗ $run"
    done
fi

# Generate consolidated results summary
if [ ${#successful_runs[@]} -gt 0 ]; then
    echo ""
    echo "Generating consolidated results..."
    
    summary_file="$RESULTS_DIR/consolidated_summary.csv"
    echo "config,test_type,total_ops,total_time_sec,throughput_ops_per_sec,mean_latency_ns,p50_latency_ns,p95_latency_ns,p99_latency_ns,p99_9_latency_ns,peak_memory_kb,cpu_cycles_per_op,instructions_per_cycle,l1_cache_miss_rate,l2_cache_miss_rate,l3_cache_miss_rate,memory_bandwidth_gb_per_sec,branch_misprediction_rate,nasdaq_file" > "$summary_file"
    
    for run in "${successful_runs[@]}"; do
        # Extract config and nasdaq_file from run name
        config_part="${run%_*}"  # Everything before the last underscore
        nasdaq_part="${run##*_}" # Everything after the last underscore
        
        config_file="$RESULTS_DIR/${config_part}_${EVENTS}_${nasdaq_part}.NASDAQ_ITCH50.csv"
        if [ -f "$config_file" ]; then
            # Add nasdaq_file column to each line
            tail -n +2 "$config_file" | while read line; do
                echo "${line},${nasdaq_part}" >> "$summary_file"
            done
        fi
    done
    
    echo "✓ Consolidated results saved to: $summary_file"
fi

# Visualization generation
if [ "$SKIP_VISUALIZATION" = false ] && [ ${#successful_runs[@]} -gt 0 ]; then
    echo ""
    echo "Generating visualization..."
    
    # Check for visualization script and Python environment
    if [ -f "visualize_results.py" ]; then
        # Try to use virtual environment if it exists
        if [ -d "venv" ]; then
            echo "Activating Python virtual environment..."
            source venv/bin/activate
            
            # Install requirements if they exist
            if [ -f "requirements.txt" ]; then
                echo "Installing/updating Python requirements..."
                pip install -r requirements.txt --quiet
            fi
        fi
        
        # Run visualization (need to provide absolute path)
        if python3 visualize_results.py "$(pwd)/$RESULTS_DIR"; then
            echo "✓ Visualization generated successfully"
            echo "  Charts saved in: $RESULTS_DIR/"
        else
            echo "⚠ Warning: Visualization generation failed"
            echo "  Results are still available in: $RESULTS_DIR/"
        fi
    else
        echo "⚠ Warning: visualize_results.py not found, skipping visualization"
    fi
else
    if [ "$SKIP_VISUALIZATION" = true ]; then
        echo "Skipping visualization (--skip-viz specified)"
    fi
fi

# Final summary
echo ""
echo "Benchmark run completed!"
echo "Results location: $RESULTS_DIR/"
echo "⏱  Total time: ${total_duration}s"
if [ "$RUN_ALL_FILES" = true ]; then
    echo "Successful runs: ${#successful_runs[@]}/$TOTAL_RUNS"
    echo "Configurations: ${#CONFIGS[@]}"
    echo "NASDAQ files: ${#NASDAQ_FILES_TO_RUN[@]}"
else
    echo "Successful configs: ${#successful_runs[@]}/${#CONFIGS[@]}"
fi

if [ -f "$RESULTS_DIR/consolidated_summary.csv" ]; then
    echo "Summary file: $RESULTS_DIR/consolidated_summary.csv"
fi

if [ ${#failed_runs[@]} -gt 0 ]; then
    echo ""
    echo "ome benchmark runs failed. Check logs above for details."
    exit 1
fi