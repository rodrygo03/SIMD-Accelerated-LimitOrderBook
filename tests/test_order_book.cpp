#include <cassert>
#include <iostream>
#include <vector>
#include "../src/order_book.h"

class OrderBookTest {
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
        void test_basic_order_operations() {
            OrderBook book;
            
            // Test initial state
            assert_test(book.get_best_bid() == 0, "Initial best bid is empty");
            assert_test(book.get_best_ask() == UINT32_MAX, "Initial best ask is empty");
            assert_test(!book.is_crossed(), "Initial book not crossed");
            
            // Test single buy order
            bool success = book.add_limit_order(1, Side::BUY, 50000, 100, 1000);
            assert_test(success, "Add buy order succeeds");
            assert_test(book.get_best_bid() == 50000, "Best bid updated");
            assert_test(book.get_best_bid_quantity() == 100, "Best bid quantity correct");
            
            // Test single sell order
            success = book.add_limit_order(2, Side::SELL, 50100, 200, 2000);
            assert_test(success, "Add sell order succeeds");
            assert_test(book.get_best_ask() == 50100, "Best ask updated");
            assert_test(book.get_best_ask_quantity() == 200, "Best ask quantity correct");
            assert_test(!book.is_crossed(), "Book not crossed with normal spread");
        }
        
        void test_order_cancellation() {
            OrderBook book;
            
            // Add orders
            book.add_limit_order(10, Side::BUY, 50000, 100, 1000);
            book.add_limit_order(11, Side::BUY, 49900, 150, 1100);
            
            // Cancel best bid
            bool success = book.cancel_order(10);
            assert_test(success, "Cancel existing order succeeds");
            assert_test(book.get_best_bid() == 49900, "Best bid updated after cancel");
            assert_test(book.get_best_bid_quantity() == 150, "New best bid quantity correct");
            
            // Try to cancel non-existent order
            success = book.cancel_order(999);
            assert_test(!success, "Cancel non-existent order fails");
            
            // Cancel remaining order
            book.cancel_order(11);
            assert_test(book.get_best_bid() == 0, "No best bid after canceling all");
        }
        
        void test_order_modification() {
            OrderBook book;
            
            book.add_limit_order(20, Side::BUY, 50000, 100, 1000);
            
            // Modify price and quantity
            bool success = book.modify_order(20, 50100, 200, 2000);
            assert_test(success, "Modify existing order succeeds");
            assert_test(book.get_best_bid() == 50100, "Price updated after modify");
            assert_test(book.get_best_bid_quantity() == 200, "Quantity updated after modify");
            
            // Try to modify non-existent order
            success = book.modify_order(999, 50000, 100, 3000);
            assert_test(!success, "Modify non-existent order fails");
        }
        
        void test_market_order_execution() {
            OrderBook book;
            std::vector<Trade> trades;
            
            // Set up order book with multiple levels
            book.add_limit_order(30, Side::SELL, 50100, 100, 1000);  // Best ask
            book.add_limit_order(31, Side::SELL, 50200, 150, 1100);  // Second level
            book.add_limit_order(32, Side::SELL, 50300, 200, 1200);  // Third level
            
            // Execute market buy that fills multiple levels
            uint32_t filled = book.execute_market_order(Side::BUY, 300, 2000, trades);
            
            assert_test(filled == 300, "Market order fully filled across levels");
            assert_test(trades.size() >= 2, "Multiple trades generated");
            assert_test(book.get_best_ask() == 50300, "Best ask updated after execution");
            // TEST FIX: Market order 300qty consumes 100+150+50=300, leaves 150 remaining
            assert_test(book.get_best_ask_quantity() == 150, "Remaining quantity correct");
        }
        
        void test_ioc_order_execution() {
            OrderBook book;
            std::vector<Trade> trades;
            
            // Set up order book
            book.add_limit_order(40, Side::BUY, 50000, 100, 1000);
            book.add_limit_order(41, Side::BUY, 49900, 200, 1100);
            
            // IOC sell at price that hits first level only
            uint32_t filled = book.execute_ioc_order(Side::SELL, 50000, 150, 2000, trades);
            
            assert_test(filled == 100, "IOC order fills available quantity");
            assert_test(book.get_best_bid() == 49900, "Best bid updated after IOC");
            
            // TEST FIX: IOC sell at 49800 vs buy at 49900 should execute (good deal for buyer)
            trades.clear();
            filled = book.execute_ioc_order(Side::SELL, 49800, 100, 3000, trades);
            assert_test(filled == 100, "IOC with good price gets fill");
            assert_test(!trades.empty(), "Trades generated for filled IOC");
        }
        
        void test_price_time_priority() {
            OrderBook book;
            std::vector<Trade> trades;
            
            // Add orders at same price - test FIFO
            book.add_limit_order(50, Side::BUY, 50000, 100, 1000);  // First
            book.add_limit_order(51, Side::BUY, 50000, 200, 1100);  // Second
            book.add_limit_order(52, Side::BUY, 50000, 150, 1200);  // Third
            
            assert_test(book.get_best_bid_quantity() == 450, "All quantities aggregated");
            
            // Market sell should hit in FIFO order
            uint32_t filled = book.execute_market_order(Side::SELL, 250, 2000, trades);
            
            assert_test(filled == 250, "Partial fill across FIFO orders");
            // Remaining quantity should be from third order (150) + partial second order (50)
            assert_test(book.get_best_bid_quantity() == 200, "FIFO order maintained");
        }
        
        void test_market_depth() {
            OrderBook book;
            
            // Build multi-level book
            book.add_limit_order(60, Side::BUY, 50000, 100, 1000);
            book.add_limit_order(61, Side::BUY, 49900, 200, 1100);
            book.add_limit_order(62, Side::BUY, 49800, 150, 1200);
            
            book.add_limit_order(63, Side::SELL, 50100, 120, 1300);
            book.add_limit_order(64, Side::SELL, 50200, 180, 1400);
            book.add_limit_order(65, Side::SELL, 50300, 250, 1500);
            
            std::vector<std::pair<uint32_t, uint32_t>> bids, asks;
            book.get_market_depth(3, bids, asks);
            
            // Test bid levels (highest to lowest)
            assert_test(bids.size() == 3, "Correct number of bid levels");
            assert_test(bids[0].first == 50000 && bids[0].second == 100, "Best bid level correct");
            assert_test(bids[1].first == 49900 && bids[1].second == 200, "Second bid level correct");
            assert_test(bids[2].first == 49800 && bids[2].second == 150, "Third bid level correct");
            
            // Test ask levels (lowest to highest)  
            assert_test(asks.size() == 3, "Correct number of ask levels");
            assert_test(asks[0].first == 50100 && asks[0].second == 120, "Best ask level correct");
            assert_test(asks[1].first == 50200 && asks[1].second == 180, "Second ask level correct");
            assert_test(asks[2].first == 50300 && asks[2].second == 250, "Third ask level correct");
        }
        
        void test_crossed_book_detection() {
            OrderBook book;
            
            book.add_limit_order(70, Side::BUY, 50000, 100, 1000);
            book.add_limit_order(71, Side::SELL, 49900, 100, 1100);  // Crossed!
            
            assert_test(book.is_crossed(), "Crossed book detected");
            assert_test(book.get_best_bid() >= book.get_best_ask(), "Bid >= Ask in crossed book");
        }
        
        void test_statistics_tracking() {
            OrderBook book;
            std::vector<Trade> trades;
            
            uint64_t initial_orders = book.get_total_orders();
            uint64_t initial_trades = book.get_total_trades();
            
            // Add some orders
            book.add_limit_order(80, Side::BUY, 50000, 100, 1000);
            book.add_limit_order(81, Side::SELL, 50100, 100, 1100);
            
            assert_test(book.get_total_orders() == initial_orders + 2, "Order count increased");
            
            // Execute trade
            book.execute_market_order(Side::BUY, 50, 2000, trades);
            
            assert_test(book.get_total_trades() > initial_trades, "Trade count increased");
            assert_test(book.get_total_volume() >= 50, "Volume tracked");
        }
        
        void test_state_management() {
            OrderBook book;
            
            // Add various orders
            book.add_limit_order(90, Side::BUY, 50000, 100, 1000);
            book.add_limit_order(91, Side::SELL, 50100, 100, 1100);
            
            // Test validation
            assert_test(book.validate_integrity(), "Book integrity maintained");
            
            // Test clear
            book.clear();
            assert_test(book.get_best_bid() == 0, "Book cleared - no bids");
            assert_test(book.get_best_ask() == UINT32_MAX, "Book cleared - no asks");
            assert_test(book.get_total_orders() == 0, "Statistics reset after clear");
        }
        
        void run_all_tests() {
            std::cout << "\n=== OrderBook Test Suite ===" << std::endl;
            
            test_basic_order_operations();
            test_order_cancellation();
            test_order_modification();
            test_market_order_execution();
            test_ioc_order_execution();
            test_price_time_priority();
            test_market_depth();
            test_crossed_book_detection();
            test_statistics_tracking();
            test_state_management();
            
            std::cout << "\nResults: " << tests_passed << "/" << tests_run << " tests passed";
            if (tests_passed == tests_run) {
                std::cout << " ✓ ALL TESTS PASSED" << std::endl;
            } else {
                std::cout << " ✗ SOME TESTS FAILED" << std::endl;
            }
        }
};

int main() {
    OrderBookTest test_suite;
    test_suite.run_all_tests();
    return 0;
}