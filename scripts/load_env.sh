#!/bin/bash
# Environment loader script for SIMD-LOB project

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Load .env file if it exists
ENV_FILE="$PROJECT_ROOT/.env"
if [ -f "$ENV_FILE" ]; then
    echo "Loading environment from $ENV_FILE"
    
    # Export variables from .env file, handling variable expansion
    while IFS='=' read -r key value; do
        # Skip comments and empty lines
        [[ $key =~ ^#.*$ ]] && continue
        [[ -z $key ]] && continue
        
        # Expand variables in the value (e.g., ${PROJECT_ROOT})
        expanded_value=$(eval echo "$value")
        
        # Export the variable
        export "$key"="$expanded_value"
        echo "  $key=$expanded_value"
    done < "$ENV_FILE"
else
    echo "Warning: .env file not found at $ENV_FILE"
    echo "Using fallback values..."
    
    # Fallback values if .env doesn't exist
    export PROJECT_ROOT="$PROJECT_ROOT"
    export BUILD_DIR="$PROJECT_ROOT/build"
    export SRC_DIR="$PROJECT_ROOT/src"
    export TEST_DIR="$PROJECT_ROOT/tests"
    export BENCH_DIR="$PROJECT_ROOT/bench"
    export SCALAR_DIR="$BENCH_DIR/scalar"
    
    # Test executables
    export BITSET_TEST_EXEC="$BUILD_DIR/test_bitset_directory"
    export ORDER_BOOK_TEST_EXEC="$BUILD_DIR/test_order_book"
    export LOB_ENGINE_TEST_EXEC="$BUILD_DIR/test_lob_engine"
    export SCALAR_TEST_EXEC="$BUILD_DIR/bench/scalar/test_scalar"
    export ALL_TESTS_EXEC="$BUILD_DIR/run_all_tests"
fi

echo "Environment loaded successfully"
echo "PROJECT_ROOT: $PROJECT_ROOT"
echo "BUILD_DIR: $BUILD_DIR"