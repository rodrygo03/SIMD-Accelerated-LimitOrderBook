cmake_minimum_required(VERSION 3.16)
project(SIMD_ACCELERATED_LOB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fsanitize=address,undefined")
    
    # Enable SIMD intrinsics
    add_compile_options(-mavx2 -mfma -mbmi2)
    
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