#include "nasdaq_itch_parser.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <arpa/inet.h>

namespace NasdaqItch {

// Network byte order conversion
uint16_t ItchParser::ntoh16(uint16_t val) const {
    return ntohs(val);
}

uint32_t ItchParser::ntoh32(uint32_t val) const {
    return ntohl(val);
}

uint64_t ItchParser::ntoh64(uint64_t val) const {
    return be64toh(val);
}

ItchParser::ItchParser(const std::string& filename) : buffer_pos(0), buffer_size(0) {
    file.open(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open ITCH file: " + filename);
    }
    
    // Allocate 1MB buffer for efficient reading
    buffer.resize(1024 * 1024);
    
    std::cout << "Opened ITCH file: " << filename << std::endl;
}

ItchParser::~ItchParser() {
    if (file.is_open()) {
        file.close();
    }
}

bool ItchParser::read_message(std::vector<uint8_t>& msg_buffer) {
    // ITCH messages are prefixed with 2-byte length
    if (buffer_pos + 2 > buffer_size) {
        // Refill buffer
        if (file.eof()) return false;
        
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        buffer_size = file.gcount();
        buffer_pos = 0;
        
        if (buffer_size < 2) return false;
    }
    
    // Read message length (big-endian)
    uint16_t msg_length = ntoh16(*reinterpret_cast<uint16_t*>(&buffer[buffer_pos]));
    buffer_pos += 2;
    
    // Ensure we have the full message
    if (buffer_pos + msg_length > buffer_size) {
        // Message spans buffer boundary, need to handle this
        msg_buffer.resize(msg_length);
        size_t bytes_available = buffer_size - buffer_pos;
        
        std::memcpy(msg_buffer.data(), &buffer[buffer_pos], bytes_available);
        
        file.read(reinterpret_cast<char*>(msg_buffer.data() + bytes_available), 
                  msg_length - bytes_available);
        
        if (file.gcount() != static_cast<std::streamsize>(msg_length - bytes_available)) {
            return false;
        }
        
        // Refill buffer for next message
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        buffer_size = file.gcount();
        buffer_pos = 0;
    } else {
        // Message fits in current buffer
        msg_buffer.resize(msg_length);
        std::memcpy(msg_buffer.data(), &buffer[buffer_pos], msg_length);
        buffer_pos += msg_length;
    }
    
    return true;
}

bool ItchParser::get_next_order_event(BenchmarkOrderEvent& event) {
    std::vector<uint8_t> msg_buffer;
    
    while (read_message(msg_buffer)) {
        if (msg_buffer.size() < sizeof(MessageHeader)) {
            continue; // Invalid message
        }
        
        MessageHeader* header = reinterpret_cast<MessageHeader*>(msg_buffer.data());
        
        // Convert header fields from network byte order
        header->stock_locate = ntoh16(header->stock_locate);
        header->tracking_number = ntoh16(header->tracking_number);
        header->timestamp = ntoh64(header->timestamp);
        
        event.timestamp_ns = header->timestamp;
        
        switch (header->message_type) {
            case MessageType::ADD_ORDER: {
                if (msg_buffer.size() < sizeof(AddOrderMessage)) continue;
                
                AddOrderMessage* add_msg = reinterpret_cast<AddOrderMessage*>(msg_buffer.data());
                add_msg->order_reference_number = ntoh64(add_msg->order_reference_number);
                add_msg->shares = ntoh32(add_msg->shares);
                add_msg->price = ntoh32(add_msg->price);
                
                event.action = BenchmarkOrderEvent::ADD;
                event.side = (add_msg->buy_sell_indicator == 'B') ? 
                            BenchmarkOrderEvent::BUY : BenchmarkOrderEvent::SELL;
                event.order_id = add_msg->order_reference_number;
                event.price = add_msg->price;
                event.quantity = add_msg->shares;
                
                // Extract symbol (remove padding)
                std::string symbol(add_msg->stock, 8);
                symbol.erase(symbol.find_last_not_of(' ') + 1);
                event.symbol = symbol;
                
                return true;
            }
            
            case MessageType::ORDER_CANCEL: {
                if (msg_buffer.size() < sizeof(OrderCancelMessage)) continue;
                
                OrderCancelMessage* cancel_msg = reinterpret_cast<OrderCancelMessage*>(msg_buffer.data());
                cancel_msg->order_reference_number = ntoh64(cancel_msg->order_reference_number);
                cancel_msg->cancelled_shares = ntoh32(cancel_msg->cancelled_shares);
                
                event.action = BenchmarkOrderEvent::CANCEL;
                event.order_id = cancel_msg->order_reference_number;
                event.quantity = cancel_msg->cancelled_shares;
                event.price = 0; // Not applicable for cancellation
                
                return true;
            }
            
            case MessageType::ORDER_DELETE: {
                if (msg_buffer.size() < sizeof(OrderDeleteMessage)) continue;
                
                OrderDeleteMessage* delete_msg = reinterpret_cast<OrderDeleteMessage*>(msg_buffer.data());
                delete_msg->order_reference_number = ntoh64(delete_msg->order_reference_number);
                
                event.action = BenchmarkOrderEvent::CANCEL;
                event.order_id = delete_msg->order_reference_number;
                event.quantity = 0; // Delete entire order
                event.price = 0;
                
                return true;
            }
            
            case MessageType::ORDER_REPLACE: {
                if (msg_buffer.size() < sizeof(OrderReplaceMessage)) continue;
                
                OrderReplaceMessage* replace_msg = reinterpret_cast<OrderReplaceMessage*>(msg_buffer.data());
                replace_msg->original_order_reference_number = ntoh64(replace_msg->original_order_reference_number);
                replace_msg->new_order_reference_number = ntoh64(replace_msg->new_order_reference_number);
                replace_msg->shares = ntoh32(replace_msg->shares);
                replace_msg->price = ntoh32(replace_msg->price);
                
                event.action = BenchmarkOrderEvent::MODIFY;
                event.order_id = replace_msg->original_order_reference_number;
                event.price = replace_msg->price;
                event.quantity = replace_msg->shares;
                
                return true;
            }
            
            case MessageType::ORDER_EXECUTED: {
                if (msg_buffer.size() < sizeof(OrderExecutedMessage)) continue;
                
                OrderExecutedMessage* exec_msg = reinterpret_cast<OrderExecutedMessage*>(msg_buffer.data());
                exec_msg->order_reference_number = ntoh64(exec_msg->order_reference_number);
                exec_msg->executed_shares = ntoh32(exec_msg->executed_shares);
                
                event.action = BenchmarkOrderEvent::EXECUTE;
                event.order_id = exec_msg->order_reference_number;
                event.quantity = exec_msg->executed_shares;
                event.price = 0; // Price not in this message type
                
                return true;
            }
            
            default:
                // Skip non-order messages
                continue;
        }
    }
    
    return false; // End of file
}

std::vector<BenchmarkOrderEvent> ItchParser::get_order_batch(size_t max_events) {
    std::vector<BenchmarkOrderEvent> events;
    events.reserve(max_events);
    
    BenchmarkOrderEvent event;
    while (events.size() < max_events && get_next_order_event(event)) {
        events.push_back(event);
    }
    
    return events;
}

void ItchParser::reset() {
    file.clear();
    file.seekg(0, std::ios::beg);
    buffer_pos = 0;
    buffer_size = 0;
}

ItchParser::FileStats ItchParser::get_file_statistics() {
    FileStats stats = {0};
    std::unordered_set<std::string> unique_symbols;
    
    // Save current position
    auto current_pos = file.tellg();
    reset();
    
    BenchmarkOrderEvent event;
    uint64_t first_timestamp = 0, last_timestamp = 0;
    bool first_event = true;
    
    while (get_next_order_event(event)) {
        stats.total_messages++;
        
        if (first_event) {
            first_timestamp = event.timestamp_ns;
            first_event = false;
        }
        last_timestamp = event.timestamp_ns;
        
        switch (event.action) {
            case BenchmarkOrderEvent::ADD:
                stats.add_orders++;
                break;
            case BenchmarkOrderEvent::CANCEL:
                stats.cancellations++;
                break;
            case BenchmarkOrderEvent::EXECUTE:
                stats.executions++;
                break;
            default:
                break;
        }
        
        if (!event.symbol.empty()) {
            unique_symbols.insert(event.symbol);
        }
    }
    
    stats.unique_symbols = unique_symbols.size();
    stats.time_span_ns = last_timestamp - first_timestamp;
    
    // Restore original position
    file.clear();
    file.seekg(current_pos);
    
    return stats;
}


}
