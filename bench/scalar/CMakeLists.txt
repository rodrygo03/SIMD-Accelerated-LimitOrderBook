# Scalar OrderBook Implementation for Benchmarking
# Purpose: Baseline comparison against SIMD-optimized implementation

# Include source directory for order.h
include_directories(../../src)

# Scalar OrderBook library
add_library(scalar_lob STATIC
    scalar_order_book.cpp
    scalar_lob_engine.cpp
)

# compile order.cpp separately since it's not part of simd_lob
add_library(scalar_order_lib STATIC ../../src/order.cpp)
target_link_libraries(scalar_lob PRIVATE scalar_order_lib)

# Test executable for scalar implementation
add_executable(test_scalar test_scalar.cpp)
target_link_libraries(test_scalar scalar_lob)

# Compiler flags for scalar version (no SIMD optimizations)
target_compile_definitions(scalar_lob PRIVATE NO_SIMD_OPTIMIZATION=1)
target_compile_options(scalar_lob PRIVATE -O2)  # Standard optimization, no -march=native

# Test target
add_test(NAME ScalarImplementationTests COMMAND test_scalar)