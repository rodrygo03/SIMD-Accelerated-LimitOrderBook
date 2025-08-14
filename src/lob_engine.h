#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <fstream>

#include "order_book.h"

enum class MessageType : uint8_t {
    ADD_ORDER = 'A',
    CANCEL_ORDER = 'C', 
    MODIFY_ORDER = 'M',
    MARKET_ORDER = 'X',
    IOC_ORDER = 'I'
};

struct OrderMessage {
    MessageType msg_type;
    uint64_t order_id;
    Side side;
    uint32_t price;
    uint32_t quantity;
    uint64_t timestamp;
    
    OrderMessage() = default;
    OrderMessage(MessageType type, uint64_t id, Side s, uint32_t p, uint32_t qty, uint64_t ts): msg_type(type), 
      order_id(id), side(s), price(p), quantity(qty), timestamp(ts) {}
};

using TradeCallback = std::function<void(const Trade&)>;
using OrderCallback = std::function<void(const Order&, const char* event)>;

class LOBEngine {
    private:
        std::unique_ptr<OrderBook> order_book; // unique_ptr for RAII 
        
        // Event callbacks
        TradeCallback trade_callback;
        OrderCallback order_callback;
        
        // Track processing metrics for latency analysis
        uint64_t messages_processed;
        uint64_t processing_time_ns;
        
        // In-memory history for deterministic replay
        std::vector<OrderMessage> message_history;
        bool record_history;
        
    public:
        explicit LOBEngine(size_t initial_pool_size = 1000000);
        ~LOBEngine() = default;
        
        // Core message processing
        bool process_message(const OrderMessage& msg);
        size_t process_batch(const std::vector<OrderMessage>& messages);
        
        // Callback registration
        void set_trade_callback(TradeCallback callback) { trade_callback = std::move(callback); }
        void set_order_callback(OrderCallback callback) { order_callback = std::move(callback); }
        
        // Direct delegation to cached OrderBook methods
        uint32_t get_best_bid() const { return order_book->get_best_bid(); }
        uint32_t get_best_ask() const { return order_book->get_best_ask(); }
        uint32_t get_best_bid_quantity() const { return order_book->get_best_bid_quantity(); }
        uint32_t get_best_ask_quantity() const { return order_book->get_best_ask_quantity(); }
        
        void get_market_depth(uint32_t levels,
                             std::vector<std::pair<uint32_t, uint32_t>>& bids,
                             std::vector<std::pair<uint32_t, uint32_t>>& asks) const;
        
        void reset();
        bool validate_state() const;
        
        // Deterministic replay
        void enable_history_recording(bool enable) { record_history = enable; }
        bool replay_history();
        bool save_history(const std::string& filename) const;
        bool load_and_replay_history(const std::string& filename);

        // Performance metrics
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
        
        // RDTSC for nanosecond precision timing
        uint64_t get_timestamp_ns() const;
};