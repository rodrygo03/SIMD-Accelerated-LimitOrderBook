# Benchmark executable CMake configuration
cmake_minimum_required(VERSION 3.16)

# Benchmark configuration variables with defaults
# Multi-file mode (6 files) contains approximately 96K total order events
# 96510 events across all nasdaq data files
set(BENCHMARK_MAX_EVENTS_PER_TEST "96510" CACHE STRING "Maximum events per test (suitable for multi-file benchmarks)")
set(BENCHMARK_ITCH_DATA_FILE "benchmark/data/01302019.NASDAQ_ITCH50" CACHE STRING "Path to NASDAQ ITCH data file")
set(BENCHMARK_SYMBOL_FILTER "" CACHE STRING "Symbol filter for benchmarks")
set(BENCHMARK_WARMUP_RATIO "0.1" CACHE STRING "Warmup events as fraction of total")
set(BENCHMARK_MIN_WARMUP_EVENTS "10" CACHE STRING "Minimum warmup events")
set(BENCHMARK_MAX_WARMUP_EVENTS "9600" CACHE STRING "Maximum warmup events (10% of total available for multi-file)")
set(BENCHMARK_THROUGHPUT_DURATION_SEC "30" CACHE STRING "Duration for throughput tests in seconds")
set(BENCHMARK_ENABLE_MEMORY_TRACKING "true" CACHE STRING "Enable memory tracking")
set(BENCHMARK_VERBOSE_OUTPUT "false" CACHE STRING "Enable verbose output")
set(BENCHMARK_RESULTS_DIR "benchmark_results" CACHE STRING "Directory for benchmark results")
set(BENCHMARK_SAVE_INDIVIDUAL_LATENCIES "false" CACHE STRING "Save raw latency data")

# Generate config header with CMake variables
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/benchmark_config.h
)

# Create comprehensive_benchmark executable with current CMake settings
add_executable(comprehensive_benchmark
    benchmark_framework.cpp
    benchmark_framework_impl.cpp
    perf_counters.cpp
    cache_flusher.cpp
    nasdaq_itch_parser.cpp
    run_comprehensive_benchmark.cpp
)

# Link with main library
target_link_libraries(comprehensive_benchmark PRIVATE simd_lob)

# Set include directories
target_include_directories(comprehensive_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Compiler flags for benchmarking
target_compile_options(comprehensive_benchmark PRIVATE
    -O3
    -march=native
    -DNDEBUG
)

# Add benchmark-specific defines and include generated config
target_compile_definitions(comprehensive_benchmark PRIVATE
    USE_CMAKE_CONFIG=1
)

# Add benchmark target to main build
set_target_properties(comprehensive_benchmark PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)


# Create data directory in build
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)

message(STATUS "Benchmark configuration:")
message(STATUS "  Max events per test: ${BENCHMARK_MAX_EVENTS_PER_TEST}")
message(STATUS "  Data file: ${BENCHMARK_ITCH_DATA_FILE}")
message(STATUS "  Results directory: ${BENCHMARK_RESULTS_DIR}")
message(STATUS "  Build directory: ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "  Data directory: ${CMAKE_CURRENT_BINARY_DIR}/data")