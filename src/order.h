#pragma once

#include <cstdint>

enum class Side: uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType: uint8_t {
    LIMIT = 0,
    MARKET = 1,
    IOC = 2  // Immediate or Cancel
};

struct Order {
    uint64_t order_id;
    uint32_t price;      // Price in ticks (e.g., cents)
    uint32_t quantity;
    uint32_t remaining_qty;
    Side side;
    OrderType type;
    uint64_t timestamp;  // Nanoseconds since epoch
    
    // Intrusive list pointers for FIFO queue
    Order* next;
    Order* prev;
    
    Order() = default;
    Order(uint64_t id, uint32_t p, uint32_t qty, Side s, OrderType t, uint64_t ts): order_id(id), 
      price(p), quantity(qty), remaining_qty(qty), side(s), type(t), timestamp(ts), next(nullptr), prev(nullptr) {}
          
    void reset(uint64_t id, uint32_t p, uint32_t qty, Side s, OrderType t, uint64_t ts);
    bool is_filled() const;
    uint32_t fill(uint32_t exec_qty);
};

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint32_t price;
    uint32_t quantity;
    uint64_t timestamp;
    
    Trade() = default;
    Trade(uint64_t buy_id, uint64_t sell_id, uint32_t p, uint32_t qty, uint64_t ts): buy_order_id(buy_id), 
      sell_order_id(sell_id), price(p), quantity(qty), timestamp(ts) {}
};