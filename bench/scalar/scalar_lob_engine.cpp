#include "scalar_lob_engine.h"
#include <chrono>
#include <fstream>
#include <immintrin.h>  // For RDTSC

ScalarLOBEngine::ScalarLOBEngine(size_t initial_pool_size): order_book(std::make_unique<ScalarOrderBook>()),
  messages_processed(0),
  processing_time_ns(0),
  record_history(false) {
    // Reserve history vector - same as optimized version
    message_history.reserve(initial_pool_size);
}

bool ScalarLOBEngine::process_message(const OrderMessage& msg) {
    uint64_t start_time = get_timestamp_ns();
    
    bool success = false;
    std::vector<Trade> trades;
    
    // Same message dispatch logic as optimized version
    switch (msg.msg_type) {
        case MessageType::ADD_ORDER:
            success = order_book->add_limit_order(msg.order_id, msg.side, msg.price, msg.quantity, msg.timestamp);
            if (success && order_callback) {
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
                notify_trade_events(trades);
            }
            break;
            
        case MessageType::IOC_ORDER:
            {
                uint32_t filled = order_book->execute_ioc_order(msg.side, msg.price, msg.quantity, msg.timestamp, trades);
                success = (filled > 0);
                notify_trade_events(trades);
            }
            break;
    }
    
    if (record_history) {
        record_message(msg);
    }
    
    uint64_t end_time = get_timestamp_ns();
    processing_time_ns += (end_time - start_time);
    messages_processed++;
    
    return success;
}

size_t ScalarLOBEngine::process_batch(const std::vector<OrderMessage>& messages) {
    size_t processed_count = 0;
    for (const auto& msg : messages) {
        if (process_message(msg)) {
            processed_count++;
        }
    }
    return processed_count;
}

void ScalarLOBEngine::get_market_depth(uint32_t levels,
  std::vector<std::pair<uint32_t, uint32_t>>& bids,
  std::vector<std::pair<uint32_t, uint32_t>>& asks) const {
    order_book->get_market_depth(levels, bids, asks);
}

void ScalarLOBEngine::reset() {
    order_book->clear();
    message_history.clear();
    reset_performance_counters();
}

bool ScalarLOBEngine::validate_state() const {
    return order_book->validate_integrity();
}

bool ScalarLOBEngine::replay_history() {
    if (message_history.empty()) {
        return false;
    }
    
    order_book->clear();
    reset_performance_counters();
    
    for (const auto& msg : message_history) {
        process_message(msg);
    }
    
    return true;
}

bool ScalarLOBEngine::save_history(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    size_t count = message_history.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    for (const auto& msg : message_history) {
        file.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
    }
    
    return file.good();
}

bool ScalarLOBEngine::load_and_replay_history(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!file.good()) {
        return false;
    }
    
    message_history.clear();
    message_history.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        OrderMessage msg;
        file.read(reinterpret_cast<char*>(&msg), sizeof(msg));
        if (!file.good()) {
            return false;
        }
        message_history.push_back(msg);
    }
    
    return replay_history();
}

double ScalarLOBEngine::get_average_latency_ns() const {
    if (messages_processed == 0) {
        return 0.0;
    }
    return static_cast<double>(processing_time_ns) / static_cast<double>(messages_processed);
}

void ScalarLOBEngine::reset_performance_counters() {
    messages_processed = 0;
    processing_time_ns = 0;
}

void ScalarLOBEngine::record_message(const OrderMessage& msg) {
    message_history.push_back(msg);
}

void ScalarLOBEngine::notify_order_event(const Order& order, const char* event) {
    if (order_callback) {
        order_callback(order, event);
    }
}

void ScalarLOBEngine::notify_trade_events(const std::vector<Trade>& trades) {
    if (trade_callback) {
        for (const auto& trade : trades) {
            trade_callback(trade);
        }
    }
}

uint64_t ScalarLOBEngine::get_timestamp_ns() const {
    // Use RDTSC for consistent high-precision timing with optimized version
    return __rdtsc();
}
