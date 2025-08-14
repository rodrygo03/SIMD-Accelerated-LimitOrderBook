#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <fstream>

#include "scalar_order_book.h"
#include "../../src/lob_engine.h"  // Import MessageType and OrderMessage

// ScalarLOBEngine: Baseline engine implementation using ScalarOrderBook
//   Provide consistent interface for benchmarking against optimized LOBEngine
//
// Key differences from optimized version:
// - Uses ScalarOrderBook (O(log n) operations vs O(1) SIMD-optimized)
// - Same message processing logic for fair comparison
// - Same callback and metrics interface
// 
class ScalarLOBEngine {
    private:
        std::unique_ptr<ScalarOrderBook> order_book;
        
        // Event callbacks - same interface as optimized version
        TradeCallback trade_callback;
        OrderCallback order_callback;
        
        // Performance tracking - same interface as optimized version
        uint64_t messages_processed;
        uint64_t processing_time_ns;
        
        // History tracking - same as optimized version
        std::vector<OrderMessage> message_history;
        bool record_history;
        
    public:
        explicit ScalarLOBEngine(size_t initial_pool_size = Config::DEFAULT_POOL_SIZE_CONFIG);
        ~ScalarLOBEngine() = default;
        
        // Core message processing - same interface as optimized version
        bool process_message(const OrderMessage& msg);
        size_t process_batch(const std::vector<OrderMessage>& messages);
        
        // Callback registration - same interface as optimized version
        void set_trade_callback(TradeCallback callback) { trade_callback = std::move(callback); }
        void set_order_callback(OrderCallback callback) { order_callback = std::move(callback); }
        
        // Direct delegation to ScalarOrderBook methods
        uint32_t get_best_bid() const { return order_book->get_best_bid(); }
        uint32_t get_best_ask() const { return order_book->get_best_ask(); }
        uint32_t get_best_bid_quantity() const { return order_book->get_best_bid_quantity(); }
        uint32_t get_best_ask_quantity() const { return order_book->get_best_ask_quantity(); }
        
        void get_market_depth(uint32_t levels,
                             std::vector<std::pair<uint32_t, uint32_t>>& bids,
                             std::vector<std::pair<uint32_t, uint32_t>>& asks) const;
        
        void reset();
        bool validate_state() const;
        
        // History tracking - same interface as optimized version
        void enable_history_recording(bool enable) { record_history = enable; }
        bool replay_history();
        bool save_history(const std::string& filename) const;
        bool load_and_replay_history(const std::string& filename);
        
        // Performance metrics - same interface as optimized version
        uint64_t get_messages_processed() const { return messages_processed; }
        uint64_t get_total_processing_time_ns() const { return processing_time_ns; }
        double get_average_latency_ns() const;
        void reset_performance_counters();
        
        uint64_t get_total_orders() const { return order_book->get_total_orders(); }
        uint64_t get_total_trades() const { return order_book->get_total_trades(); }
        uint64_t get_total_volume() const { return order_book->get_total_volume(); }
        
    private:
        void record_message(const OrderMessage& msg);
        void notify_order_event(const Order& order, const char* event);
        void notify_trade_events(const std::vector<Trade>& trades);
        
        // RDTSC timing for consistent benchmarking with optimized version  
        uint64_t get_timestamp_ns() const;
};
