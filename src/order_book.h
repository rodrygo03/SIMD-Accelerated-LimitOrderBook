#pragma once

#include "price_level.hpp"
#include "object_pool.hpp"

#include <unordered_map>
#include <vector>
#include <cstdint>

#include "bitset_directory.h"
#include "order.h"

class OrderBook {
    private:
        static constexpr uint32_t MAX_PRICE_LEVELS = 4096;
        static constexpr uint32_t MIN_PRICE_TICK = 1;  // Minimum price increment
        static constexpr uint32_t BASE_PRICE = 50000;  // Base price (e.g., $500.00 in cents) 
        
        // Cache optimization: 64-byte aligned arrays for cache line efficiency
        alignas(64) PriceLevel buy_levels[MAX_PRICE_LEVELS];   
        alignas(64) PriceLevel sell_levels[MAX_PRICE_LEVELS]; 
        
        BitsetDirectory buy_directory;   // Tracks which buy levels have orders
        BitsetDirectory sell_directory;  // Tracks which sell levels have orders
        
        OrderPool order_pool;
        TradePool trade_pool;
        
        std::unordered_map<uint64_t, Order*> order_map; // Order ID to Order* mapping for O(1) cancel/modify
        
        // Cached best price indices to avoid repeated SIMD scans
        mutable uint32_t cached_best_bid_idx;   // Cached best bid index
        mutable uint32_t cached_best_ask_idx;   // Cached best ask index
        mutable bool best_bid_valid;            // Cache validity flag for bid
        mutable bool best_ask_valid;            // Cache validity flag for ask
        
        uint64_t total_orders_processed;
        uint64_t total_trades_executed;
        uint64_t total_volume_traded;
        
    public:
        explicit OrderBook(size_t initial_pool_size = 1000000); // *Make 1000000 a macro ?
        ~OrderBook() = default;

        // Core order operations
        bool add_limit_order(uint64_t order_id, Side side, uint32_t price, uint32_t quantity, uint64_t timestamp);
        bool cancel_order(uint64_t order_id, uint64_t timestamp);
        bool modify_order(uint64_t order_id, uint32_t new_price, uint32_t new_quantity, uint64_t timestamp);
        uint32_t execute_market_order(Side side, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);  
        uint32_t execute_ioc_order(Side side, uint32_t price, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);
        
        // Best price queries (O(1) with bitset directory)
        uint32_t get_best_bid() const;
        uint32_t get_best_ask() const;
        uint32_t get_best_bid_quantity() const;
        uint32_t get_best_ask_quantity() const;

        bool is_crossed() const; // book is crossed (bid >= ask)

        // Market data access
        const PriceLevel* get_price_level(Side side, uint32_t price) const;
        void get_market_depth(uint32_t levels, 
            std::vector<std::pair<uint32_t, uint32_t>>& bids,
            std::vector<std::pair<uint32_t, uint32_t>>& asks) const;
        
        // State management
        void clear();
        bool validate_integrity() const;
        
        // Statistics
        uint64_t get_total_orders() const { return total_orders_processed; }
        uint64_t get_total_trades() const { return total_trades_executed; }
        uint64_t get_total_volume() const { return total_volume_traded; }
        void reset_statistics();
        
    private:
        uint32_t price_to_buy_index(uint32_t price) const;
        uint32_t price_to_sell_index(uint32_t price) const;
        uint32_t buy_index_to_price(uint32_t index) const;
        uint32_t sell_index_to_price(uint32_t index) const;
        
        // Lazy cache invalidation and refresh
        void invalidate_best_prices();          // Mark cached values as stale
        void refresh_best_bid_cache() const;    // Recompute and cache best bid
        void refresh_best_ask_cache() const;    // Recompute and cache best ask
        
        void execute_trade(Order* buy_order, Order* sell_order, uint32_t price,
                        uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);
        
        void remove_order_from_level(Order* order, Side side);
        void prefetch_price_levels(Side side, uint32_t start_index, uint32_t count) const; // Hardware cache prefetching
};
