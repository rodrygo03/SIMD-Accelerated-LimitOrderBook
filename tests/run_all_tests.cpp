#include <iostream>
#include <cstdlib>

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

// Test runner wrapper functions
int run_bitset_directory_tests() {
    return system("cd /home/rodrigoorozco/Desktop/SIMD-LOB && ./build/tests/test_bitset_directory");
}

int run_order_book_tests() {
    return system("cd /home/rodrigoorozco/Desktop/SIMD-LOB && ./build/tests/test_order_book");
}

int run_lob_engine_tests() {
    return system("cd /home/rodrigoorozco/Desktop/SIMD-LOB && ./build/tests/test_lob_engine");
}