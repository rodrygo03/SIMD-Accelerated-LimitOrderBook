#include <iostream>
#include <cstdlib>
#include <string>

// Forward declarations for test runner functions
int run_bitset_directory_tests();
int run_order_book_tests();  
int run_lob_engine_tests();

int main() {
    std::cout << "SIMD-LOB Test Suite Runner" << std::endl;
    std::cout << "===========================" << std::endl;
    
    int total_failures = 0;
    
    total_failures += run_bitset_directory_tests();
    total_failures += run_order_book_tests();
    total_failures += run_lob_engine_tests();
    
    std::cout << "\n=== FINAL RESULTS ===" << std::endl;
    if (total_failures == 0) {
        std::cout << "✓ ALL TEST SUITES PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "✗ " << total_failures << " TEST SUITE(S) FAILED" << std::endl;
        return 1;
    }
}

// Environment-aware test runner using configurable paths
std::string get_project_root() {
    const char* project_root = std::getenv("PROJECT_ROOT");
    if (project_root) {
        return std::string(project_root);
    }
    // Fallback to hardcoded path if environment variable not set
    return "/home/rodrigoorozco/Desktop/SIMD-LOB";
}

std::string get_build_dir() {
    const char* build_dir = std::getenv("BUILD_DIR");
    if (build_dir) {
        return std::string(build_dir);
    }
    return get_project_root() + "/build";
}

int run_bitset_directory_tests() {
    const char* test_exec = std::getenv("BITSET_TEST_EXEC");
    if (test_exec) {
        return system(test_exec);
    }
    std::string command = "cd " + get_project_root() + " && " + get_build_dir() + "/test_bitset_directory";
    return system(command.c_str());
}

int run_order_book_tests() {
    const char* test_exec = std::getenv("ORDER_BOOK_TEST_EXEC");
    if (test_exec) {
        return system(test_exec);
    }
    std::string command = "cd " + get_project_root() + " && " + get_build_dir() + "/test_order_book";
    return system(command.c_str());
}

int run_lob_engine_tests() {
    const char* test_exec = std::getenv("LOB_ENGINE_TEST_EXEC");
    if (test_exec) {
        return system(test_exec);
    }
    std::string command = "cd " + get_project_root() + " && " + get_build_dir() + "/test_lob_engine";
    return system(command.c_str());
}