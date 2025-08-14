#pragma once

#include "../../src/config.h"
#include "../../src/order.h"
#include <map>
#include <list>
#include <unordered_map>
#include <vector>
#include <cstdint>

// ScalarOrderBook: Baseline implementation using standard STL containers
//   Provide performance comparison baseline against SIMD-optimized OrderBook
// 
// Key differences from optimized version:
// - Uses std::map for price levels (O(log n) vs O(1) bitset directory)
// - Uses std::list for order queues (pointer chasing vs intrusive lists)  
// - Uses new/delete for orders (heap allocation vs object pooling)
// - No cache optimization (scattered allocation vs 64-byte aligned arrays)
// - No SIMD instructions (scalar operations vs AVX2 bitset scanning)
// - No hardware prefetching hints
class ScalarOrderBook {
    private:
        // STL containers for price levels - O(log n) access vs O(1) array indexing
        std::map<uint32_t, std::list<Order*>> buy_levels;   // Higher prices first (reverse order)
        std::map<uint32_t, std::list<Order*>> sell_levels;  // Lower prices first (natural order)
        
        // Still need O(1) order lookup for cancellation 
        std::unordered_map<uint64_t, Order*> order_map;
        
        // Statistics tracking - same as optimized version
        uint64_t total_orders_processed;
        uint64_t total_trades_executed; 
        uint64_t total_volume_traded;
        
        // Constants from config.h - same as optimized version for fair comparison
        static constexpr uint32_t MAX_PRICE_LEVELS = Config::BitsetConfig::MAX_PRICE_LEVELS;
        static constexpr uint32_t MIN_PRICE_TICK = Config::MIN_PRICE_TICK_CONFIG;
        static constexpr uint32_t BASE_PRICE = Config::BASE_PRICE_CONFIG;
        
    public:
        ScalarOrderBook();
        ~ScalarOrderBook();
        
        // Core order operations - same interface as optimized version
        bool add_limit_order(uint64_t order_id, Side side, uint32_t price, uint32_t quantity, uint64_t timestamp);
        bool cancel_order(uint64_t order_id);
        bool modify_order(uint64_t order_id, uint32_t new_price, uint32_t new_quantity, uint64_t timestamp);
        uint32_t execute_market_order(Side side, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);
        uint32_t execute_ioc_order(Side side, uint32_t price, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);
        
        // Best price queries - O(log n) via std::map iteration vs O(1) bitset directory
        uint32_t get_best_bid() const;
        uint32_t get_best_ask() const;
        uint32_t get_best_bid_quantity() const;
        uint32_t get_best_ask_quantity() const;
        
        bool is_crossed() const;
        
        // Market data access - linear search through maps vs direct array access
        void get_market_depth(uint32_t levels,
                             std::vector<std::pair<uint32_t, uint32_t>>& bids,
                             std::vector<std::pair<uint32_t, uint32_t>>& asks) const;
        
        // State management
        void clear();
        bool validate_integrity() const;
        
        // Statistics - same interface as optimized version
        uint64_t get_total_orders() const { return total_orders_processed; }
        uint64_t get_total_trades() const { return total_trades_executed; }
        uint64_t get_total_volume() const { return total_volume_traded; }
        void reset_statistics();
        
    private:
        // Helper methods for order execution and management
        void remove_order_from_level(Order* order, Side side);
        uint32_t execute_orders_at_level(std::list<Order*>& order_queue, uint32_t quantity, 
          uint32_t price, uint64_t timestamp, std::vector<Trade>& trades);
        
        // Trade execution helper
        void execute_trade(Order* buy_order, Order* sell_order, uint32_t price,
          uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades);
};
