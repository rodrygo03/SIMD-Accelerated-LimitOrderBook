#!/bin/bash
# SIMD Function Analysis Script
# Analyzes specific SIMD functions to identify bottlenecks

set -e

PROFILE_DATA=${1:-"profiling_results/simd_profile.data"}

echo "SIMD Function Analysis"
echo "====================="
echo "Profile data: $PROFILE_DATA"
echo ""

if [ ! -f "$PROFILE_DATA" ]; then
    echo "Error: Profile data not found. Run ./profile_simd.sh first"
    exit 1
fi

echo "=== Top Functions by CPU Usage ==="
perf report -i "$PROFILE_DATA" --stdio --sort=overhead | head -20

echo ""
echo "=== SIMD-Specific Functions ==="
echo "Looking for SIMD-related functions..."

# Search for SIMD function names
SIMD_FUNCTIONS=(
    "find_next_higher_bit"
    "find_next_lower_bit" 
    "simd_scan"
    "mm256"
    "_mm256"
    "avx"
    "vector"
)

for func in "${SIMD_FUNCTIONS[@]}"; do
    echo ""
    echo "--- Function: $func ---"
    perf report -i "$PROFILE_DATA" --stdio --symbol-filter="*$func*" | head -10
done

echo ""
echo "=== Assembly Analysis of Top SIMD Functions ==="
echo "Analyzing assembly for SIMD hotspots..."

# Get top SIMD functions and analyze their assembly
TOP_SIMD_FUNCS=$(perf report -i "$PROFILE_DATA" --stdio --symbol-filter="*find_next*" | grep -E "^\s*[0-9.]+%" | head -3 | awk '{print $3}')

for func in $TOP_SIMD_FUNCS; do
    if [ ! -z "$func" ]; then
        echo ""
        echo "--- Assembly for $func ---"
        perf annotate -i "$PROFILE_DATA" --symbol="$func" --stdio | head -20
    fi
done

echo ""
echo "=== Cache Analysis ==="
echo "Analyzing cache behavior in SIMD code..."

# Check for cache misses in specific functions
perf report -i "$PROFILE_DATA" --stdio --sort=overhead,symbol | grep -E "(find_next|simd)" | head -10

echo ""
echo "=== Call Graph Analysis ==="
echo "Analyzing call patterns for SIMD functions..."

perf report -i "$PROFILE_DATA" --stdio --call-graph --symbol-filter="*find_next*" | head -30

echo ""
echo "Analysis complete!"
echo ""
echo "Manual Investigation Commands:"
echo "1. Interactive view: perf report -i $PROFILE_DATA"
echo "2. Focus on function: perf report -i $PROFILE_DATA --symbol-filter=find_next_higher_bit"
echo "3. Assembly analysis: perf annotate -i $PROFILE_DATA --symbol=find_next_higher_bit"
echo "4. Call graph: perf report -i $PROFILE_DATA --call-graph=graph,0.5"