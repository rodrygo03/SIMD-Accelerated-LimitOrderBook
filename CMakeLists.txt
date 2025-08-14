cmake_minimum_required(VERSION 3.16)
project(SIMD_ACCELERATED_LOB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# BUILD CONFIGURATION OPTIONS
# ============================================================================

# Build options
option(BUILD_SCALAR_BASELINE "Build scalar baseline implementation for benchmarking" ON)
option(ENABLE_SIMD "Enable SIMD optimizations (AVX2)" ON)
option(ENABLE_DEBUG_MODE "Enable debug mode and logging" OFF)
option(ENABLE_VERBOSE_LOGGING "Enable verbose logging output" OFF)

# Pool configuration
set(DEFAULT_POOL_SIZE "1000000" CACHE STRING "Default object pool size")
set(TRADE_POOL_RATIO "10" CACHE STRING "Trade pool size ratio (orders/trades)")

# Order book configuration  
set(MAX_PRICE_LEVELS "4096" CACHE STRING "Maximum price levels (up to 4096 for SIMD)")
set(BASE_PRICE "50000" CACHE STRING "Base price in cents (e.g., 50000 = $500.00)")
set(MIN_PRICE_TICK "1" CACHE STRING "Minimum price increment")
set(MAX_MARKET_DEPTH_LEVELS "10" CACHE STRING "Default market depth query levels")

# Benchmark configuration
set(BENCHMARK_ITERATIONS "10000" CACHE STRING "Default benchmark iterations")
set(WARMUP_ITERATIONS "1000" CACHE STRING "Benchmark warmup iterations")

# ============================================================================
# CONFIGURATION VALIDATION
# ============================================================================

# Validate pool sizes
if(DEFAULT_POOL_SIZE LESS 1000)
    message(FATAL_ERROR "DEFAULT_POOL_SIZE must be at least 1000")
endif()

if(TRADE_POOL_RATIO LESS 1)
    message(FATAL_ERROR "TRADE_POOL_RATIO must be at least 1")
endif()

# Validate price configuration
if(MAX_PRICE_LEVELS GREATER 4096)
    message(FATAL_ERROR "MAX_PRICE_LEVELS cannot exceed 4096 (SIMD bitset directory limit)")
endif()

if(BASE_PRICE LESS 1)
    message(FATAL_ERROR "BASE_PRICE must be positive")
endif()

if(MIN_PRICE_TICK LESS 1)
    message(FATAL_ERROR "MIN_PRICE_TICK must be positive")
endif()

# Validate benchmark settings
if(BENCHMARK_ITERATIONS LESS 100)
    message(FATAL_ERROR "BENCHMARK_ITERATIONS must be at least 100")
endif()

# Display configuration
message(STATUS "SIMD-LOB Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SIMD Enabled: ${ENABLE_SIMD}")
message(STATUS "  Scalar Baseline: ${BUILD_SCALAR_BASELINE}")
message(STATUS "  Pool Size: ${DEFAULT_POOL_SIZE}")
message(STATUS "  Base Price: ${BASE_PRICE} cents ($${CMAKE_MATCH_0})")
message(STATUS "  Max Price Levels: ${MAX_PRICE_LEVELS}")
message(STATUS "  Debug Mode: ${ENABLE_DEBUG_MODE}")

# ============================================================================
# COMPILER CONFIGURATION
# ============================================================================

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fsanitize=address,undefined")
    
    # Enable SIMD intrinsics if requested
    if(ENABLE_SIMD)
        add_compile_options(-mavx2 -mfma -mbmi2)
    endif()
    
    # Performance-oriented flags
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -ffast-math
        -fno-signed-zeros
        -fno-trapping-math
        -funroll-loops
        -fprefetch-loop-arrays
    )
endif()

# ============================================================================
# CONFIGURATION HEADER GENERATION
# ============================================================================

# Set preprocessor definitions for config.h template
if(ENABLE_DEBUG_MODE)
    set(SIMD_LOB_DEBUG 1)
endif()

if(ENABLE_VERBOSE_LOGGING)
    set(VERBOSE_LOGGING 1)
endif()

# Generate config.h from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
    @ONLY
)

# Make generated config.h available to all targets
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")

include_directories(src)

# Core library
add_library(simd_lob STATIC
    src/order.cpp
    src/price_level.cpp
    src/bitset_directory.cpp
    src/order_book.cpp
    src/lob_engine.cpp
)

# Custom target to run all testsinclude_directories(src)

add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_bitset_directory test_order_book test_lob_engine
    COMMENT "Running all tests"
)

# Individual test executables
enable_testing()

add_executable(test_bitset_directory tests/test_bitset_directory.cpp)
target_link_libraries(test_bitset_directory simd_lob)
add_test(NAME BitsetDirectoryTests COMMAND test_bitset_directory)

add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book simd_lob)
add_test(NAME OrderBookTests COMMAND test_order_book)

add_executable(test_lob_engine tests/test_lob_engine.cpp)
target_link_libraries(test_lob_engine simd_lob)
add_test(NAME LOBEngineTests COMMAND test_lob_engine)

# Combined test runner
add_executable(run_all_tests tests/run_all_tests.cpp)
add_test(NAME AllTests COMMAND run_all_tests)

# Install targets
install(TARGETS simd_lob test_bitset_directory test_order_book test_lob_engine run_all_tests
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY src/ DESTINATION include/simd_lob)

# Add scalar implementation for benchmarking 
option(BUILD_SCALAR_BASELINE "Build scalar baseline implementation" ON)
if(BUILD_SCALAR_BASELINE)
    add_subdirectory(bench/scalar)
endif()