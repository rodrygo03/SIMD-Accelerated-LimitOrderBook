#include <chrono>
#include <fstream>
#include <immintrin.h>

#include "lob_engine.h"

LOBEngine::LOBEngine(size_t initial_pool_size): order_book(std::make_unique<OrderBook<OptimizationConfig::DefaultConfig>>(initial_pool_size)),
  messages_processed(0),
  processing_time_ns(0),
  record_history(false) {
    
    // Reserve history vector to avoid reallocation
    message_history.reserve(initial_pool_size);
}

bool LOBEngine::process_message(const OrderMessage& msg) {
    uint64_t start_time = get_timestamp_ns();
    
    bool success = false;
    std::vector<Trade> trades;
    
    // Switch-based message dispatch for predictable branching
    switch (msg.msg_type) {
        case MessageType::ADD_ORDER:
            success = order_book->add_limit_order(msg.order_id, msg.side, msg.price, msg.quantity, msg.timestamp);
            if (success && order_callback) {
                // Avoid order lookup by creating temporary order
                Order temp_order(msg.order_id, msg.price, msg.quantity, msg.side, OrderType::LIMIT, msg.timestamp);
                notify_order_event(temp_order, "added");
            }
            break;
            
        case MessageType::CANCEL_ORDER:
            success = order_book->cancel_order(msg.order_id);
            if (success && order_callback) {
                Order temp_order(msg.order_id, 0, 0, msg.side, OrderType::LIMIT, msg.timestamp);
                notify_order_event(temp_order, "cancelled");
            }
            break;
            
        case MessageType::MODIFY_ORDER:
            success = order_book->modify_order(msg.order_id, msg.price, msg.quantity, msg.timestamp);
            if (success && order_callback) {
                Order temp_order(msg.order_id, msg.price, msg.quantity, msg.side, OrderType::LIMIT, msg.timestamp);
                notify_order_event(temp_order, "modified");
            }
            break;
            
        case MessageType::MARKET_ORDER:
            {
                uint32_t filled = order_book->execute_market_order(msg.side, msg.quantity, msg.timestamp, trades);
                success = (filled > 0);
                if (success && !trades.empty()) {
                    notify_trade_events(trades);
                }
            }
            break;
            
        case MessageType::IOC_ORDER:
            {
                uint32_t filled = order_book->execute_ioc_order(msg.side, msg.price, msg.quantity, msg.timestamp, trades);
                success = (filled > 0);
                if (success && !trades.empty()) {
                    notify_trade_events(trades);
                }
            }
            break;
    }
    
    // Conditional history recording to avoid unnecessary copies
    if (record_history && success) {
        record_message(msg);
    }
    
    // accumulate processing time
    uint64_t end_time = get_timestamp_ns();
    processing_time_ns += (end_time - start_time);
    messages_processed++;
    
    return success;
}

size_t LOBEngine::process_batch(const std::vector<OrderMessage>& messages) {
    // Batch processing reduces function call overhead
    size_t processed_count = 0;
    
    // Reserve space for potential trades
    std::vector<Trade> batch_trades;
    batch_trades.reserve(messages.size() / 4); // Estimate 25% fill rate
    
    for (const auto& msg : messages) {
        if (process_message(msg)) {
            processed_count++;
        }
    }
    
    return processed_count;
}

void LOBEngine::get_market_depth(uint32_t levels,
  std::vector<std::pair<uint32_t, uint32_t>>& bids,
  std::vector<std::pair<uint32_t, uint32_t>>& asks) const {
    // Direct delegation to OrderBook's SIMD-optimized implementation
    order_book->get_market_depth(levels, bids, asks);
}

void LOBEngine::reset() {
    order_book->clear();
    message_history.clear();
    reset_performance_counters();
}

bool LOBEngine::validate_state() const {
    return order_book->validate_integrity();
}

bool LOBEngine::replay_history() {
    if (message_history.empty()) {
        return true;
    }
    
    // Clear state before replay for deterministic results
    order_book->clear();
    reset_performance_counters();
    
    // Disable history recording during replay to avoid duplication
    bool old_record_state = record_history;
    record_history = false;
    
    size_t processed = 0;
    for (const auto& msg : message_history) {
        if (process_message(msg)) {
            processed++;
        }
    }
    
    record_history = old_record_state;
    return processed == message_history.size();
}

bool LOBEngine::save_history(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header with count for efficient loading
    size_t count = message_history.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Bulk write of contiguous data
    if (count > 0) {
        file.write(reinterpret_cast<const char*>(message_history.data()), 
                   count * sizeof(OrderMessage));
    }
    
    return file.good();
}

bool LOBEngine::load_and_replay_history(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    if (!file.good() || count == 0) {
        return false;
    }
    
    // Reserve exact space needed
    message_history.clear();
    message_history.resize(count);
    
    // Bulk read of contiguous data
    file.read(reinterpret_cast<char*>(message_history.data()), 
              count * sizeof(OrderMessage));
    
    if (!file.good()) {
        message_history.clear();
        return false;
    }
    
    return replay_history();
}

double LOBEngine::get_average_latency_ns() const {
    if (messages_processed == 0) {
        return 0.0;
    }
    return static_cast<double>(processing_time_ns) / messages_processed;
}

void LOBEngine::reset_performance_counters() {
    messages_processed = 0;
    processing_time_ns = 0;
}

void LOBEngine::record_message(const OrderMessage& msg) {
    // Direct push_back, vector is pre-reserved
    message_history.push_back(msg);
}

void LOBEngine::notify_order_event(const Order& order, const char* event) {
    // Null check before callback to avoid function call overhead
    if (order_callback) {
        order_callback(order, event);
    }
}

void LOBEngine::notify_trade_events(const std::vector<Trade>& trades) {
    // Batch notification to reduce callback overhead
    if (trade_callback) {
        for (const auto& trade : trades) {
            trade_callback(trade);
        }
    }
}

uint64_t LOBEngine::get_timestamp_ns() const {
    return __rdtsc();
}
