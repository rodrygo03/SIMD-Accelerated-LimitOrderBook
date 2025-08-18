#!/bin/bash
# SIMD Performance Profiling Script (debugging)
# Usage: ./profile_simd.sh [events] [comparison_config]

set -e

EVENTS=${1:-10000}
COMPARE_CONFIG=${2:-"scalar-baseline"}

echo "SIMD Performance Profiling"
echo "=========================="
echo "Events: $EVENTS"
echo "Comparing: simd-baseline vs $COMPARE_CONFIG"
echo ""

# Check if we're in the right directory
if [ ! -f "../CMakePresets.json" ]; then
    echo "Error: This script must be run from the benchmark directory"
    exit 1
fi

# Check if perf is available
if ! command -v perf &> /dev/null; then
    echo "Error: perf command not found. Install with:"
    echo "  sudo dnf install perf  # Fedora"
    echo "  sudo apt install linux-perf  # Ubuntu"
    exit 1
fi

# Check perf permissions
if ! perf stat true &>/dev/null; then
    echo "Error: perf requires elevated permissions. Run:"
    echo "  sudo sysctl kernel.perf_event_paranoid=1"
    echo "Or run this script with sudo"
    exit 1
fi

cd ..

# Build both configurations
echo "Building configurations..."
echo "Building $COMPARE_CONFIG..."
cmake --preset="$COMPARE_CONFIG" > /dev/null 2>&1
cmake --build --preset="$COMPARE_CONFIG" -j2 > /dev/null 2>&1

echo "Building simd-baseline..."
cmake --preset="simd-baseline" > /dev/null 2>&1
cmake --build --preset="simd-baseline" -j2 > /dev/null 2>&1

echo "✓ Builds complete"
echo ""

# Create profiling results directory
PROFILE_DIR="benchmark/profiling_results"
mkdir -p "$PROFILE_DIR"

echo "=== Profiling $COMPARE_CONFIG ==="
echo "Running detailed perf analysis..."

# Set correct data file path
export ITCH_DATA_FILE="$(pwd)/benchmark/data/01302019.NASDAQ_ITCH50"

# Profile scalar baseline with detailed metrics
cd "build-$COMPARE_CONFIG"
perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses \
    -o "../$PROFILE_DIR/${COMPARE_CONFIG}_perf_stat.txt" \
    ./benchmark/comprehensive_benchmark "$COMPARE_CONFIG" "$EVENTS" 2>&1 | tee "../$PROFILE_DIR/${COMPARE_CONFIG}_output.txt"

echo "✓ $COMPARE_CONFIG profiling complete"
cd ..

echo ""
echo "=== Profiling simd-baseline ==="
echo "Running detailed perf analysis..."

# Profile SIMD with detailed metrics
cd "build-simd-baseline"
perf stat -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses \
    -o "../$PROFILE_DIR/simd_baseline_perf_stat.txt" \
    ./benchmark/comprehensive_benchmark "simd-baseline" "$EVENTS" 2>&1 | tee "../$PROFILE_DIR/simd_baseline_output.txt"

echo "✓ simd-baseline profiling complete"
cd ..

echo ""
echo "=== Advanced SIMD Profiling ==="

# Profile SIMD with function-level details
echo "Recording function-level profile for SIMD..."
cd "build-simd-baseline"
perf record -g --call-graph=dwarf -o "../$PROFILE_DIR/simd_profile.data" \
    ./benchmark/comprehensive_benchmark "simd-baseline" "$EVENTS" > /dev/null 2>&1

echo "✓ SIMD function profile recorded"
cd ..

# Generate detailed reports
echo ""
echo "=== Generating Analysis Reports ==="

# Create performance comparison
cat > "$PROFILE_DIR/analysis_summary.txt" << EOF
SIMD Performance Analysis Report
===============================
Generated: $(date)
Events: $EVENTS
Configurations: simd-baseline vs $COMPARE_CONFIG

Key Files:
- ${COMPARE_CONFIG}_perf_stat.txt: Detailed hardware counters for $COMPARE_CONFIG
- simd_baseline_perf_stat.txt: Detailed hardware counters for SIMD
- simd_profile.data: Function-level profiling data for SIMD
- ${COMPARE_CONFIG}_output.txt: Benchmark output for $COMPARE_CONFIG
- simd_baseline_output.txt: Benchmark output for SIMD

Analysis Commands:
==================

1. View function hotspots in SIMD:
   perf report -i $PROFILE_DIR/simd_profile.data

2. Compare cache performance:
   grep -E "(cache-misses|cache-references)" $PROFILE_DIR/*_perf_stat.txt

3. Compare instruction efficiency:
   grep -E "(cycles|instructions)" $PROFILE_DIR/*_perf_stat.txt

4. Analyze SIMD instruction usage:
   perf annotate -i $PROFILE_DIR/simd_profile.data

5. View call graph:
   perf report -i $PROFILE_DIR/simd_profile.data --call-graph

Automated Analysis:
==================
EOF

# Extract key metrics for comparison
echo "" >> "$PROFILE_DIR/analysis_summary.txt"
echo "Performance Comparison:" >> "$PROFILE_DIR/analysis_summary.txt"
echo "=====================" >> "$PROFILE_DIR/analysis_summary.txt"

# Parse throughput from outputs
SCALAR_THROUGHPUT=$(grep -o "Throughput: [0-9.]* ops/sec" "$PROFILE_DIR/${COMPARE_CONFIG}_output.txt" | head -1 | grep -o "[0-9.]*" || echo "N/A")
SIMD_THROUGHPUT=$(grep -o "Throughput: [0-9.]* ops/sec" "$PROFILE_DIR/simd_baseline_output.txt" | head -1 | grep -o "[0-9.]*" || echo "N/A")

echo "$COMPARE_CONFIG throughput: $SCALAR_THROUGHPUT ops/sec" >> "$PROFILE_DIR/analysis_summary.txt"
echo "simd-baseline throughput: $SIMD_THROUGHPUT ops/sec" >> "$PROFILE_DIR/analysis_summary.txt"

if [[ "$SCALAR_THROUGHPUT" != "N/A" && "$SIMD_THROUGHPUT" != "N/A" ]]; then
    PERF_RATIO=$(echo "scale=2; $SIMD_THROUGHPUT / $SCALAR_THROUGHPUT" | bc -l 2>/dev/null || echo "N/A")
    echo "SIMD/Scalar ratio: ${PERF_RATIO}x" >> "$PROFILE_DIR/analysis_summary.txt"
fi

echo "" >> "$PROFILE_DIR/analysis_summary.txt"

# Extract and compare key perf metrics
echo "Hardware Counter Comparison:" >> "$PROFILE_DIR/analysis_summary.txt"
echo "===========================" >> "$PROFILE_DIR/analysis_summary.txt"

for metric in "cycles" "instructions" "cache-misses" "branch-misses" "L1-dcache-load-misses"; do
    echo "" >> "$PROFILE_DIR/analysis_summary.txt"
    echo "$metric:" >> "$PROFILE_DIR/analysis_summary.txt"
    grep -E "^\s*[0-9,.]+ +$metric" "$PROFILE_DIR/${COMPARE_CONFIG}_perf_stat.txt" | sed "s/^/  $COMPARE_CONFIG: /" >> "$PROFILE_DIR/analysis_summary.txt" 2>/dev/null || echo "  $COMPARE_CONFIG: N/A" >> "$PROFILE_DIR/analysis_summary.txt"
    grep -E "^\s*[0-9,.]+ +$metric" "$PROFILE_DIR/simd_baseline_perf_stat.txt" | sed "s/^/  simd-baseline: /" >> "$PROFILE_DIR/analysis_summary.txt" 2>/dev/null || echo "  simd-baseline: N/A" >> "$PROFILE_DIR/analysis_summary.txt"
done

echo ""
echo "Profiling Complete!"
echo ""
echo "Results saved to: $PROFILE_DIR/"
echo ""
echo "Quick Analysis:"
cat "$PROFILE_DIR/analysis_summary.txt" | tail -20

echo ""
echo "Next Steps:"
echo "1. View full analysis: cat $PROFILE_DIR/analysis_summary.txt"
echo "2. Interactive function analysis: perf report -i $PROFILE_DIR/simd_profile.data"
echo "3. SIMD instruction analysis: perf annotate -i $PROFILE_DIR/simd_profile.data"
echo "4. Check specific functions: perf report -i $PROFILE_DIR/simd_profile.data --symbol-filter=<function_name>"