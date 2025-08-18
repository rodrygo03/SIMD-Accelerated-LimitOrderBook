#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <memory>

namespace NasdaqItch {

// NASDAQ ITCH 5.0 Message Types
enum class MessageType : char {
    SYSTEM_EVENT = 'S',
    STOCK_DIRECTORY = 'R', 
    STOCK_TRADING_ACTION = 'H',
    REG_SHO_RESTRICTION = 'Y',
    MARKET_PARTICIPANT_POSITION = 'L',
    MWCB_DECLINE_LEVEL = 'V',
    MWCB_BREACH = 'W',
    IPO_QUOTING_PERIOD_UPDATE = 'K',
    LULD_AUCTION_COLLAR = 'J',
    OPERATIONAL_HALT = 'h',
    ADD_ORDER = 'A',           // Add Order - No MPID Attribution
    ADD_ORDER_MPID = 'F',      // Add Order - MPID Attribution  
    ORDER_EXECUTED = 'E',      // Order Executed
    ORDER_EXECUTED_WITH_PRICE = 'C', // Order Executed With Price
    ORDER_CANCEL = 'X',        // Order Cancel
    ORDER_DELETE = 'D',        // Order Delete
    ORDER_REPLACE = 'U',       // Order Replace
    TRADE = 'P',              // Trade (Non-Cross)
    CROSS_TRADE = 'Q',        // Cross Trade
    BROKEN_TRADE = 'B',       // Broken Trade
    NOII = 'I'               // Net Order Imbalance Indicator
};

// Base message header (all messages start with this)
struct MessageHeader {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;        // Nanoseconds since midnight
    MessageType message_type;
} __attribute__((packed));

// Add Order Message (Type 'A')
struct AddOrderMessage {
    MessageHeader header;
    uint64_t order_reference_number;
    char buy_sell_indicator;   // 'B' = Buy, 'S' = Sell
    uint32_t shares;
    char stock[8];            // Right padded with spaces
    uint32_t price;           // Price in 1/10000 dollars
} __attribute__((packed));

// Order Executed Message (Type 'E')  
struct OrderExecutedMessage {
    MessageHeader header;
    uint64_t order_reference_number;
    uint32_t executed_shares;
    uint64_t match_number;
} __attribute__((packed));

// Order Cancel Message (Type 'X')
struct OrderCancelMessage {
    MessageHeader header;
    uint64_t order_reference_number;
    uint32_t cancelled_shares;
} __attribute__((packed));

// Order Delete Message (Type 'D')
struct OrderDeleteMessage {
    MessageHeader header;
    uint64_t order_reference_number;
} __attribute__((packed));

// Order Replace Message (Type 'U')
struct OrderReplaceMessage {
    MessageHeader header;
    uint64_t original_order_reference_number;
    uint64_t new_order_reference_number;
    uint32_t shares;
    uint32_t price;
} __attribute__((packed));

// Benchmark-friendly order event
struct BenchmarkOrderEvent {
    enum Action { ADD, CANCEL, MODIFY, EXECUTE };
    enum Side { BUY, SELL };
    
    Action action;
    Side side;
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    uint64_t timestamp_ns;
    std::string symbol;
};

class ItchParser {
private:
    std::ifstream file;
    std::vector<uint8_t> buffer;
    size_t buffer_pos;
    size_t buffer_size;
    
    // Network byte order conversion
    uint16_t ntoh16(uint16_t val) const;
    uint32_t ntoh32(uint32_t val) const; 
    uint64_t ntoh64(uint64_t val) const;
    
    bool read_message(std::vector<uint8_t>& msg_buffer);
    
public:
    ItchParser(const std::string& filename);
    ~ItchParser();
    
    // Parse next message and convert to benchmark event
    // Returns false when end of file reached
    bool get_next_order_event(BenchmarkOrderEvent& event);
    
    std::vector<BenchmarkOrderEvent> get_order_batch(size_t max_events = 10000);
    
    // TODO: Skip to specific symbol (for focused testing)
    void filter_symbol(const std::string& symbol);
    
    void reset();
    
    struct FileStats {
        size_t total_messages;
        size_t add_orders;
        size_t cancellations; 
        size_t executions;
        size_t unique_symbols;
        uint64_t time_span_ns;
    };
    
    FileStats get_file_statistics();
};


}