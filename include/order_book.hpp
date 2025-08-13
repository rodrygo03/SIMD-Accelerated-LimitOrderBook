#pragma once
#include "order.hpp"
#include "price_level.hpp"
#include "bitset_directory.hpp"
#include "object_pool.hpp"
#include <unordered_map>
#include <vector>
#include <cstdint>

class OrderBook {
private:
    static constexpr uint32_t MAX_PRICE_LEVELS = 4096;
    static constexpr uint32_t MIN_PRICE_TICK = 1;    // Minimum price increment
    static constexpr uint32_t BASE_PRICE = 50000;    // Base price (e.g., $500.00 in cents)
    
    // Fixed price ladder - cache-aligned for performance
    alignas(64) PriceLevel buy_levels[MAX_PRICE_LEVELS];   // Index 0 = highest buy price
    alignas(64) PriceLevel sell_levels[MAX_PRICE_LEVELS];  // Index 0 = lowest sell price
    
    // Two-level bitset directories for O(1) best price discovery
    BitsetDirectory buy_directory;   // Tracks which buy levels have orders
    BitsetDirectory sell_directory;  // Tracks which sell levels have orders
    
    // Object pools for zero-allocation operation
    OrderPool order_pool;
    TradePool trade_pool;
    
    // Order ID to Order* mapping for O(1) cancel/modify
    std::unordered_map<uint64_t, Order*> order_map;
    
    // Current best bid/ask cache
    mutable uint32_t cached_best_bid_idx;
    mutable uint32_t cached_best_ask_idx;
    mutable bool best_bid_valid;
    mutable bool best_ask_valid;
    
    // Statistics
    uint64_t total_orders_processed;
    uint64_t total_trades_executed;
    uint64_t total_volume_traded;
    
public:
    explicit OrderBook(size_t initial_pool_size = 1000000);
    ~OrderBook() = default;
    
    // Core order operations
    // TODO: Add limit order to book, return true if successful
    bool add_limit_order(uint64_t order_id, Side side, uint32_t price, 
                        uint32_t quantity, uint64_t timestamp);
    
    // TODO: Cancel existing order, return true if found and cancelled
    bool cancel_order(uint64_t order_id, uint64_t timestamp);
    
    // TODO: Modify existing order quantity/price, return true if successful
    bool modify_order(uint64_t order_id, uint32_t new_price, 
                     uint32_t new_quantity, uint64_t timestamp);
    
    // TODO: Execute market order, return filled quantity and generate trades
    uint32_t execute_market_order(Side side, uint32_t quantity, 
                                 uint64_t timestamp, std::vector<Trade>& trades);
    
    // TODO: Execute IOC order, return filled quantity
    uint32_t execute_ioc_order(Side side, uint32_t price, uint32_t quantity,
                              uint64_t timestamp, std::vector<Trade>& trades);
    
    // Best price queries (O(1) with bitset directory)
    // TODO: Get best bid price, return 0 if no bids
    uint32_t get_best_bid() const;
    
    // TODO: Get best ask price, return UINT32_MAX if no asks  
    uint32_t get_best_ask() const;
    
    // TODO: Get total quantity at best bid level
    uint32_t get_best_bid_quantity() const;
    
    // TODO: Get total quantity at best ask level
    uint32_t get_best_ask_quantity() const;
    
    // TODO: Check if book is crossed (bid >= ask)
    bool is_crossed() const;
    
    // Market data access
    // TODO: Get price level information for given side and price
    const PriceLevel* get_price_level(Side side, uint32_t price) const;
    
    // TODO: Get snapshot of top N levels for each side
    void get_market_depth(uint32_t levels, 
                         std::vector<std::pair<uint32_t, uint32_t>>& bids,
                         std::vector<std::pair<uint32_t, uint32_t>>& asks) const;
    
    // State management
    // TODO: Clear all orders and reset to empty state
    void clear();
    
    // TODO: Validate book integrity (all invariants)
    bool validate_integrity() const;
    
    // Statistics
    uint64_t get_total_orders() const { return total_orders_processed; }
    uint64_t get_total_trades() const { return total_trades_executed; }
    uint64_t get_total_volume() const { return total_volume_traded; }
    
    // TODO: Reset all statistics counters
    void reset_statistics();
    
private:
    // TODO: Convert price to array index for buy side (higher price = lower index)
    uint32_t price_to_buy_index(uint32_t price) const;
    
    // TODO: Convert price to array index for sell side (lower price = lower index)  
    uint32_t price_to_sell_index(uint32_t price) const;
    
    // TODO: Convert buy index back to price
    uint32_t buy_index_to_price(uint32_t index) const;
    
    // TODO: Convert sell index back to price
    uint32_t sell_index_to_price(uint32_t index) const;
    
    // TODO: Update best bid/ask cache after order book change
    void invalidate_best_prices();
    
    // TODO: Refresh best bid cache using SIMD directory scan
    void refresh_best_bid_cache() const;
    
    // TODO: Refresh best ask cache using SIMD directory scan  
    void refresh_best_ask_cache() const;
    
    // TODO: Execute trade matching between buy and sell orders
    void execute_trade(Order* buy_order, Order* sell_order, uint32_t price,
                      uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);
    
    // TODO: Remove order from price level and update directories
    void remove_order_from_level(Order* order, Side side);
    
    // TODO: Prefetch likely-to-be-accessed cache lines
    void prefetch_price_levels(Side side, uint32_t start_index, uint32_t count) const;
};