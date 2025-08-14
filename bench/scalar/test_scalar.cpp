#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include "scalar_order_book.h"
#include "scalar_lob_engine.h"

// Simple test to verify scalar implementation correctness
void test_scalar_order_book() {
    std::cout << "Testing ScalarOrderBook basic functionality...\n";
    
    ScalarOrderBook book;
    
    // Test 1: Initial state
    assert(book.get_best_bid() == 0);
    assert(book.get_best_ask() == 0);
    assert(!book.is_crossed());
    
    // Test 2: Add orders
    assert(book.add_limit_order(1, Side::BUY, 50000, 100, 1000));
    assert(book.get_best_bid() == 50000);
    assert(book.get_best_bid_quantity() == 100);
    
    assert(book.add_limit_order(2, Side::SELL, 50100, 150, 1001));
    assert(book.get_best_ask() == 50100);
    assert(book.get_best_ask_quantity() == 150);
    
    // Test 3: Not crossed
    assert(!book.is_crossed());
    
    // Test 4: Cancel order
    assert(book.cancel_order(1));
    assert(book.get_best_bid() == 0);
    
    // Test 5: Market order execution
    book.add_limit_order(3, Side::BUY, 50000, 100, 1002);
    book.add_limit_order(4, Side::BUY, 49900, 200, 1003);
    
    std::vector<Trade> trades;
    uint32_t filled = book.execute_market_order(Side::SELL, 250, 1004, trades);
    assert(filled == 250);  // Should fill across both levels
    assert(trades.size() == 2);  // Two trades generated
    
    std::cout << "✓ ScalarOrderBook basic tests passed\n";
}

void test_scalar_lob_engine() {
    std::cout << "Testing ScalarLOBEngine basic functionality...\n";
    
    ScalarLOBEngine engine(10000);
    
    // Test message processing
    OrderMessage add_msg(MessageType::ADD_ORDER, 1, Side::BUY, 50000, 100, 1000);
    assert(engine.process_message(add_msg));
    
    assert(engine.get_best_bid() == 50000);
    assert(engine.get_messages_processed() == 1);
    
    OrderMessage cancel_msg(MessageType::CANCEL_ORDER, 1, Side::BUY, 0, 0, 1001);
    assert(engine.process_message(cancel_msg));
    
    assert(engine.get_best_bid() == 0);
    assert(engine.get_messages_processed() == 2);
    
    std::cout << "✓ ScalarLOBEngine basic tests passed\n";
}

void performance_comparison_sample() {
    std::cout << "\nRunning simple performance comparison...\n";
    
    ScalarOrderBook scalar_book;
    const int num_orders = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add orders
    for (int i = 0; i < num_orders; ++i) {
        scalar_book.add_limit_order(i, Side::BUY, 50000 - (i % 100), 100, i);
    }
    
    // Query best prices
    for (int i = 0; i < 1000; ++i) {
        volatile uint32_t bid = scalar_book.get_best_bid();  // Prevent optimization
        (void)bid;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "ScalarOrderBook: " << num_orders << " orders + 1000 best_bid queries: " 
              << duration.count() << " μs\n";
    std::cout << "Average per operation: " << (duration.count() * 1000.0) / (num_orders + 1000) << " ns\n";
}

int main() {
    std::cout << "=== Scalar Implementation Test Suite ===\n";
    
    try {
        test_scalar_order_book();
        test_scalar_lob_engine();
        performance_comparison_sample();
        
        std::cout << "\nAll scalar implementation tests passed!\n";
        std::cout << "\nNext steps:\n";
        std::cout << "1. Add to CMakeLists.txt for compilation\n";
        std::cout << "2. Create comprehensive benchmark suite\n";
        std::cout << "3. Compare against optimized SIMD implementation\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}