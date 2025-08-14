#!/bin/bash
# Enhanced build and test script using CMake presets

set -e  # Exit on any error

# Load environment variables for paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$SCRIPT_DIR/load_env.sh"

echo "=== SIMD-LOB Enhanced Build and Test Script ==="
echo "Project Root: $PROJECT_ROOT"
echo ""

# Determine CMake preset to use
PRESET="${1:-${DEFAULT_CMAKE_PRESET:-default}}"
echo "Using CMake preset: $PRESET"

# Configure with CMake preset
echo "Configuring project with CMake preset '$PRESET'..."
cd "$PROJECT_ROOT"

if ! cmake --preset="$PRESET"; then
    echo "Failed to configure with preset '$PRESET'"
    echo "Available presets:"
    cmake --list-presets=configure 2>/dev/null || echo "  (No presets available)"
    exit 1
fi

# Get build directory from preset
BUILD_DIR_FROM_PRESET=$(cmake --preset="$PRESET" --dry-run 2>/dev/null | grep "Binary directory" | cut -d: -f2 | xargs)
if [ -n "$BUILD_DIR_FROM_PRESET" ]; then
    BUILD_DIR="$BUILD_DIR_FROM_PRESET"
fi

echo "Build Directory: $BUILD_DIR"

# Build the project using CMake build preset
echo ""
echo "Building SIMD ACCELERATED LOB system..."
if ! cmake --build --preset="$PRESET" -j$(nproc); then
    echo "Build failed"
    exit 1
fi

echo ""
echo "Build complete. Running tests..."

# Update test executable paths for the current build directory
BITSET_TEST_EXEC="$BUILD_DIR/test_bitset_directory"
ORDER_BOOK_TEST_EXEC="$BUILD_DIR/test_order_book"
LOB_ENGINE_TEST_EXEC="$BUILD_DIR/test_lob_engine"
SCALAR_TEST_EXEC="$BUILD_DIR/bench/scalar/test_scalar"
ALL_TESTS_EXEC="$BUILD_DIR/run_all_tests"

# Function to run a test with error checking
run_test() {
    local test_name="$1"
    local test_exec="$2"
    
    echo ""
    echo "Running $test_name tests..."
    
    if [ -x "$test_exec" ]; then
        if "$test_exec"; then
            echo "$test_name tests passed"
            return 0
        else
            echo "$test_name tests failed"
            return 1
        fi
    else
        echo "$test_name test executable not found: $test_exec"
        return 1
    fi
}

# Track test results
total_tests=0
passed_tests=0

# Run individual tests
tests=(
    "BitsetDirectory:$BITSET_TEST_EXEC"
    "OrderBook:$ORDER_BOOK_TEST_EXEC"
    "LOBEngine:$LOB_ENGINE_TEST_EXEC"
    "ScalarImplementation:$SCALAR_TEST_EXEC"
)

for test_info in "${tests[@]}"; do
    IFS=':' read -r test_name test_exec <<< "$test_info"
    total_tests=$((total_tests + 1))
    
    if run_test "$test_name" "$test_exec"; then
        passed_tests=$((passed_tests + 1))
    fi
done

# Run CTest with preset if available
echo ""
echo "Running CTest with preset..."
if ctest --preset="$PRESET" --output-on-failure 2>/dev/null; then
    echo "✓ CTest passed"
elif [ -f "$BUILD_DIR/CTestTestfile.cmake" ]; then
    echo "Running CTest in build directory..."
    cd "$BUILD_DIR"
    if ctest --output-on-failure; then
        echo "CTest passed"
    else
        echo "CTest failed"
    fi
else
    echo "CTest not available"
fi

# Summary
echo ""
echo "=== Test Summary ==="
echo "CMake preset: $PRESET"
echo "Build directory: $BUILD_DIR"
echo "Tests passed: $passed_tests/$total_tests"

if [ "$passed_tests" -eq "$total_tests" ]; then
    echo "All tests passed!"
    echo ""
    echo "Build artifacts:"
    echo "  - Optimized SIMD library: $BUILD_DIR/libsimd_lob.a"
    echo "  - Scalar baseline library: $BUILD_DIR/bench/scalar/libscalar_lob.a"
    echo "  - Test executables available in: $BUILD_DIR"
    echo ""
    echo "Configuration used (from preset '$PRESET'):"
    echo "  - Check CMakeCache.txt in $BUILD_DIR for full configuration"
    exit 0
else
    echo "Some tests failed"
    exit 1
fi