#include <cassert>
#include <iostream>
#include <vector>

#include "../src/bitset_directory.h"

class BitsetDirectoryTest {
    private:
        int tests_run = 0;
        int tests_passed = 0;
        
        void assert_test(bool condition, const char* test_name) {
            tests_run++;
            if (condition) {
                tests_passed++;
                std::cout << "✓ " << test_name << std::endl;
            } else {
                std::cout << "✗ " << test_name << " FAILED" << std::endl;
            }
        }
        
    public:
        void test_basic_operations() {
            BitsetDirectory bd;
            
            // Test initial state - all bits should be clear
            assert_test(!bd.has_any_bits(), "Initial state is empty");
            assert_test(bd.find_highest_bit() == 4096, "No highest bit when empty");
            assert_test(bd.find_lowest_bit() == 4096, "No lowest bit when empty");
            
            // Test single bit operations
            bd.set_bit(100);
            assert_test(bd.test_bit(100), "Set bit is readable");
            assert_test(!bd.test_bit(99), "Adjacent bit not set");
            assert_test(bd.has_any_bits(), "Has bits after setting");
            
            bd.clear_bit(100);
            assert_test(!bd.test_bit(100), "Cleared bit is not readable");
            assert_test(!bd.has_any_bits(), "No bits after clearing all");
        }
        
        void test_find_operations() {
            BitsetDirectory bd;
            
            // Test single bit scenarios
            bd.set_bit(500);
            assert_test(bd.find_highest_bit() == 500, "Single bit highest");
            assert_test(bd.find_lowest_bit() == 500, "Single bit lowest");
            
            // Test multiple bits - performance test for SIMD
            bd.set_bit(100);  // Lower
            bd.set_bit(2000); // Higher
            
            assert_test(bd.find_highest_bit() == 2000, "Multiple bits - highest correct");
            assert_test(bd.find_lowest_bit() == 100, "Multiple bits - lowest correct");
            
            // Test edge cases
            bd.clear_all();
            bd.set_bit(0);     // First possible bit
            bd.set_bit(4095);  // Last possible bit
            
            assert_test(bd.find_highest_bit() == 4095, "Edge case - last bit");
            assert_test(bd.find_lowest_bit() == 0, "Edge case - first bit");
        }
        
        void test_next_bit_operations() {
            BitsetDirectory bd;
            
            // Set up test pattern: bits at 100, 200, 300
            bd.set_bit(100);
            bd.set_bit(200);
            bd.set_bit(300);
            
            // Test find_next_higher_bit
            assert_test(bd.find_next_higher_bit(150) == 200, "Next higher from middle");
            assert_test(bd.find_next_higher_bit(99) == 100, "Next higher from below");
            assert_test(bd.find_next_higher_bit(300) == 4096, "Next higher from last");
            
            // Test find_next_lower_bit  
            assert_test(bd.find_next_lower_bit(250) == 200, "Next lower from middle");
            assert_test(bd.find_next_lower_bit(101) == 100, "Next lower from above");
            assert_test(bd.find_next_lower_bit(50) == 4096, "Next lower from before first");
        }
        
        void test_simd_scan_operations() {
            BitsetDirectory bd;
            
            // Test SIMD forward scan - spread bits across different L1 chunks
            bd.set_bit(64);   // Chunk 1
            bd.set_bit(128);  // Chunk 2  
            bd.set_bit(256);  // Chunk 4
            
            uint32_t result = bd.simd_scan_l2_forward(0);
            assert_test(result == 64, "SIMD forward scan finds first bit");
            
            result = bd.simd_scan_l2_forward(100);
            assert_test(result == 128, "SIMD forward scan from middle");
            
            // Test SIMD backward scan
            result = bd.simd_scan_l2_backward(300);
            assert_test(result == 256, "SIMD backward scan finds last bit");
            
            result = bd.simd_scan_l2_backward(200);
            assert_test(result == 128, "SIMD backward scan from middle");
        }
        
        void test_consistency_validation() {
            BitsetDirectory bd;
            
            // Test empty state consistency
            assert_test(bd.validate_consistency(), "Empty state is consistent");
            
            // Test with various bit patterns
            bd.set_bit(42);
            bd.set_bit(1337);
            bd.set_bit(3000);
            assert_test(bd.validate_consistency(), "Multiple bits state is consistent");
            
            // Test after clear operations
            bd.clear_bit(1337);
            assert_test(bd.validate_consistency(), "After clear operation is consistent");
            
            bd.clear_all();
            assert_test(bd.validate_consistency(), "After clear_all is consistent");
        }
        
        void test_performance_patterns() {
            BitsetDirectory bd;
            
            // Test dense bit pattern (stress test for L2 chunks)
            for (uint32_t i = 1000; i < 1064; i++) {
                bd.set_bit(i);
            }
            
            assert_test(bd.find_highest_bit() == 1063, "Dense pattern - highest");
            assert_test(bd.find_lowest_bit() == 1000, "Dense pattern - lowest");
            
            // Test sparse pattern (stress test for SIMD scanning)
            bd.clear_all();
            bd.set_bit(1);
            bd.set_bit(1000);
            bd.set_bit(2000);
            bd.set_bit(4000);
            
            assert_test(bd.find_highest_bit() == 4000, "Sparse pattern - highest");
            assert_test(bd.find_lowest_bit() == 1, "Sparse pattern - lowest");
            
            // Verify all bits are still accessible
            assert_test(bd.test_bit(1) && bd.test_bit(1000) && 
                       bd.test_bit(2000) && bd.test_bit(4000), "Sparse pattern - all bits accessible");
        }
        
        void run_all_tests() {
            std::cout << "\n=== BitsetDirectory Test Suite ===" << std::endl;
            
            test_basic_operations();
            test_find_operations();
            test_next_bit_operations();
            test_simd_scan_operations();
            test_consistency_validation();
            test_performance_patterns();
            
            std::cout << "\nResults: " << tests_passed << "/" << tests_run << " tests passed";
            if (tests_passed == tests_run) {
                std::cout << " ✓ ALL TESTS PASSED" << std::endl;
            } else {
                std::cout << " ✗ SOME TESTS FAILED" << std::endl;
            }
        }
};

int main() {
    BitsetDirectoryTest test_suite;
    test_suite.run_all_tests();
    return 0;
}