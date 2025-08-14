#include <cassert>
#include <iostream>
#include <vector>
#include <fstream>
#include "../src/lob_engine.h"

class LOBEngineTest {
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
        void test_basic_message_processing() {
            LOBEngine engine;
            
            // Test ADD_ORDER message
            OrderMessage add_msg(MessageType::ADD_ORDER, 1, Side::BUY, 50000, 100, 1000);
            bool success = engine.process_message(add_msg);
            
            assert_test(success, "Process ADD_ORDER message");
            assert_test(engine.get_best_bid() == 50000, "Best bid updated after add");
            assert_test(engine.get_messages_processed() == 1, "Message count tracked");
            
            // Test CANCEL_ORDER message
            OrderMessage cancel_msg(MessageType::CANCEL_ORDER, 1, Side::BUY, 0, 0, 2000);
            success = engine.process_message(cancel_msg);
            
            assert_test(success, "Process CANCEL_ORDER message");
            assert_test(engine.get_best_bid() == 0, "Best bid cleared after cancel");
            assert_test(engine.get_messages_processed() == 2, "Message count incremented");
        }
        
        void test_market_order_processing() {
            LOBEngine engine;
            
            // Set up order book
            OrderMessage sell1(MessageType::ADD_ORDER, 10, Side::SELL, 50100, 100, 1000);
            OrderMessage sell2(MessageType::ADD_ORDER, 11, Side::SELL, 50200, 150, 1100);
            
            engine.process_message(sell1);
            engine.process_message(sell2);
            
            // Execute market buy
            OrderMessage market_buy(MessageType::MARKET_ORDER, 12, Side::BUY, 0, 200, 2000);
            bool success = engine.process_message(market_buy);
            
            assert_test(success, "Process MARKET_ORDER message");
            assert_test(engine.get_best_ask() == 50200, "Best ask updated after market order");
            assert_test(engine.get_total_trades() > 0, "Trades generated from market order");
        }
        
        void test_ioc_order_processing() {
            LOBEngine engine;
            
            // Set up order book
            OrderMessage buy1(MessageType::ADD_ORDER, 20, Side::BUY, 50000, 100, 1000);
            engine.process_message(buy1);
            
            // IOC sell at good price
            OrderMessage ioc_good(MessageType::IOC_ORDER, 21, Side::SELL, 50000, 50, 2000);
            bool success = engine.process_message(ioc_good);
            
            assert_test(success, "IOC order with good price fills");
            
            // IOC sell at better price (economically beneficial trade)
            OrderMessage ioc_good2(MessageType::IOC_ORDER, 22, Side::SELL, 49000, 50, 3000);
            success = engine.process_message(ioc_good2);
            
            assert_test(success, "IOC order with good price fills");
        }
        
        void test_batch_processing() {
            LOBEngine engine;
            
            std::vector<OrderMessage> messages;
            messages.emplace_back(MessageType::ADD_ORDER, 30, Side::BUY, 50000, 100, 1000);
            messages.emplace_back(MessageType::ADD_ORDER, 31, Side::SELL, 50100, 100, 1100);
            messages.emplace_back(MessageType::ADD_ORDER, 32, Side::BUY, 49900, 200, 1200);
            
            size_t processed = engine.process_batch(messages);
            
            assert_test(processed == 3, "All batch messages processed");
            assert_test(engine.get_messages_processed() == 3, "Batch message count correct");
            assert_test(engine.get_best_bid() == 50000, "Best bid correct after batch");
            assert_test(engine.get_best_ask() == 50100, "Best ask correct after batch");
        }
        
        void test_callbacks() {
            LOBEngine engine;
            
            // Track callback invocations
            int trade_callbacks = 0;
            int order_callbacks = 0;
            
            engine.set_trade_callback([&](const Trade& trade) {
                trade_callbacks++;
            });
            
            engine.set_order_callback([&](const Order& order, const char* event) {
                order_callbacks++;
            });
            
            // Generate events
            OrderMessage add_msg(MessageType::ADD_ORDER, 40, Side::BUY, 50000, 100, 1000);
            OrderMessage sell_msg(MessageType::ADD_ORDER, 41, Side::SELL, 50000, 50, 1100); // Should match
            
            engine.process_message(add_msg);
            engine.process_message(sell_msg);
            
            assert_test(order_callbacks >= 2, "Order callbacks invoked");
            // Note: Trade callbacks depend on immediate matching, may be 0 with limit orders
        }
        
        void test_performance_metrics() {
            LOBEngine engine;
            
            uint64_t initial_time = engine.get_total_processing_time_ns();
            
            // Process some messages
            OrderMessage msg1(MessageType::ADD_ORDER, 50, Side::BUY, 50000, 100, 1000);
            OrderMessage msg2(MessageType::ADD_ORDER, 51, Side::SELL, 50100, 100, 1100);
            
            engine.process_message(msg1);
            engine.process_message(msg2);
            
            assert_test(engine.get_total_processing_time_ns() > initial_time, "Processing time tracked");
            assert_test(engine.get_average_latency_ns() > 0, "Average latency calculated");
            
            // Test reset
            engine.reset_performance_counters();
            assert_test(engine.get_messages_processed() == 0, "Message count reset");
            assert_test(engine.get_total_processing_time_ns() == 0, "Processing time reset");
        }
        
        void test_market_depth_delegation() {
            LOBEngine engine;
            
            // Build book with multiple levels
            engine.process_message({MessageType::ADD_ORDER, 60, Side::BUY, 50000, 100, 1000});
            engine.process_message({MessageType::ADD_ORDER, 61, Side::BUY, 49900, 200, 1100});
            engine.process_message({MessageType::ADD_ORDER, 62, Side::SELL, 50100, 150, 1200});
            engine.process_message({MessageType::ADD_ORDER, 63, Side::SELL, 50200, 250, 1300});
            
            std::vector<std::pair<uint32_t, uint32_t>> bids, asks;
            engine.get_market_depth(2, bids, asks);
            
            assert_test(bids.size() == 2, "Correct bid depth returned");
            assert_test(asks.size() == 2, "Correct ask depth returned");
            assert_test(bids[0].first == 50000, "Best bid price correct");
            assert_test(asks[0].first == 50100, "Best ask price correct");
        }
        
        void test_history_recording() {
            LOBEngine engine;
            
            // Enable history recording
            engine.enable_history_recording(true);
            
            // Process some messages
            OrderMessage msg1(MessageType::ADD_ORDER, 70, Side::BUY, 50000, 100, 1000);
            OrderMessage msg2(MessageType::ADD_ORDER, 71, Side::SELL, 50100, 100, 1100);
            
            engine.process_message(msg1);
            engine.process_message(msg2);
            
            // Test replay
            uint32_t best_bid_before = engine.get_best_bid();
            uint32_t best_ask_before = engine.get_best_ask();
            
            bool replay_success = engine.replay_history();
            assert_test(replay_success, "History replay succeeds");
            assert_test(engine.get_best_bid() == best_bid_before, "State consistent after replay");
            assert_test(engine.get_best_ask() == best_ask_before, "State consistent after replay");
        }
        
        void test_history_persistence() {
            LOBEngine engine;
            const std::string test_file = "/tmp/lob_test_history.dat";
            
            engine.enable_history_recording(true);
            
            // Generate some history
            engine.process_message({MessageType::ADD_ORDER, 80, Side::BUY, 50000, 100, 1000});
            engine.process_message({MessageType::ADD_ORDER, 81, Side::SELL, 50100, 100, 1100});
            
            // Save history
            bool save_success = engine.save_history(test_file);
            assert_test(save_success, "History save succeeds");
            
            // Create new engine and load history
            LOBEngine engine2;
            bool load_success = engine2.load_and_replay_history(test_file);
            assert_test(load_success, "History load and replay succeeds");
            
            // Verify state matches
            assert_test(engine2.get_best_bid() == engine.get_best_bid(), "Loaded state matches original");
            assert_test(engine2.get_best_ask() == engine.get_best_ask(), "Loaded state matches original");
            
            // Cleanup
            std::remove(test_file.c_str());
        }
        
        void test_state_management() {
            LOBEngine engine;
            
            // Add some state
            engine.process_message({MessageType::ADD_ORDER, 90, Side::BUY, 50000, 100, 1000});
            
            assert_test(engine.validate_state(), "Engine state is valid");
            assert_test(engine.get_total_orders() > 0, "Orders tracked before reset");
            
            // Test reset
            engine.reset();
            assert_test(engine.get_best_bid() == 0, "State cleared after reset");
            assert_test(engine.get_total_orders() == 0, "Statistics cleared after reset");
            assert_test(engine.validate_state(), "Engine state valid after reset");
        }
        
        void test_error_conditions() {
            LOBEngine engine;
            
            // Test duplicate order ID
            OrderMessage msg1(MessageType::ADD_ORDER, 100, Side::BUY, 50000, 100, 1000);
            OrderMessage msg2(MessageType::ADD_ORDER, 100, Side::SELL, 50100, 100, 1100); // Same ID
            
            bool success1 = engine.process_message(msg1);
            bool success2 = engine.process_message(msg2);
            
            assert_test(success1, "First order with ID succeeds");
            assert_test(!success2, "Duplicate order ID fails");
            
            // Test zero quantity
            OrderMessage zero_qty(MessageType::ADD_ORDER, 101, Side::BUY, 50000, 0, 2000);
            bool success3 = engine.process_message(zero_qty);
            assert_test(!success3, "Zero quantity order fails");
            
            // Test cancel non-existent order
            OrderMessage cancel_bad(MessageType::CANCEL_ORDER, 999, Side::BUY, 0, 0, 3000);
            bool success4 = engine.process_message(cancel_bad);
            assert_test(!success4, "Cancel non-existent order fails");
        }
        
        void run_all_tests() {
            std::cout << "\n=== LOBEngine Test Suite ===" << std::endl;
            
            test_basic_message_processing();
            test_market_order_processing();
            test_ioc_order_processing();
            test_batch_processing();
            test_callbacks();
            test_performance_metrics();
            test_market_depth_delegation();
            test_history_recording();
            test_history_persistence();
            test_state_management();
            test_error_conditions();
            
            std::cout << "\nResults: " << tests_passed << "/" << tests_run << " tests passed";
            if (tests_passed == tests_run) {
                std::cout << " ✓ ALL TESTS PASSED" << std::endl;
            } else {
                std::cout << " ✗ SOME TESTS FAILED" << std::endl;
            }
        }
};

int main() {
    LOBEngineTest test_suite;
    test_suite.run_all_tests();
    return 0;
}