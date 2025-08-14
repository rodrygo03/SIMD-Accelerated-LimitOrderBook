#pragma once

#include "config.h"
#include "optimization_config.h"
#include "price_level.h"
#include "object_pool.hpp"

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <type_traits>

#include "bitset_directory.h"
#include "order.h"

template<typename Config = OptimizationConfig::DefaultConfig>
class OrderBook {
    private:
        // Configuration from config.h - environment configurable
        static constexpr uint32_t MAX_PRICE_LEVELS = ::Config::BitsetConfig::MAX_PRICE_LEVELS;
        static constexpr uint32_t MIN_PRICE_TICK = ::Config::MIN_PRICE_TICK_CONFIG;
        static constexpr uint32_t BASE_PRICE = ::Config::BASE_PRICE_CONFIG; 
        
        // Cache optimization: 64-byte aligned arrays for cache line efficiency
        alignas(Config::USE_CACHE_OPTIMIZATION ? 64 : alignof(PriceLevel)) PriceLevel buy_levels[MAX_PRICE_LEVELS];   
        alignas(Config::USE_CACHE_OPTIMIZATION ? 64 : alignof(PriceLevel)) PriceLevel sell_levels[MAX_PRICE_LEVELS]; 
        
        BitsetDirectory<Config> buy_directory;   
        BitsetDirectory<Config> sell_directory;
        
        OrderPool order_pool;
        TradePool trade_pool;
        
        std::unordered_map<uint64_t, Order*> order_map; // Order ID to Order* mapping for O(1) cancel/modify
        
        // Cached best price indices to avoid repeated SIMD scans
        mutable uint32_t cached_best_bid_idx;
        mutable uint32_t cached_best_ask_idx;
        mutable bool best_bid_valid;            // Cache validity flag for bid
        mutable bool best_ask_valid;            // Cache validity flag for ask
        
        uint64_t total_orders_processed;
        uint64_t total_trades_executed;
        uint64_t total_volume_traded;
        
    public:
        explicit OrderBook(size_t initial_pool_size = ::Config::DEFAULT_POOL_SIZE_CONFIG);
        ~OrderBook() = default;

        // Core order operations
        bool add_limit_order(uint64_t order_id, Side side, uint32_t price, uint32_t quantity, uint64_t timestamp);
        bool cancel_order(uint64_t order_id);
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

// ============================================================================
// TYPE ALIASES FOR COMMON CONFIGURATIONS
// ============================================================================

// Main OrderBook type - uses configuration selected by CMake
using OrderBookDefault = OrderBook<OptimizationConfig::DefaultConfig>;

// Specific configuration aliases for benchmarking
using FullyOptimizedOrderBook = OrderBook<OptimizationConfig::FullyOptimizedConfig>;
using ScalarBaselineOrderBook = OrderBook<OptimizationConfig::ScalarBaselineConfig>;
using SimdOnlyOrderBook = OrderBook<OptimizationConfig::SimdOnlyConfig>;
using MemoryOptimizedOrderBook = OrderBook<OptimizationConfig::MemoryOptimizedConfig>;
using CacheOptimizedOrderBook = OrderBook<OptimizationConfig::CacheOptimizedConfig>;
using ObjectPoolOnlyOrderBook = OrderBook<OptimizationConfig::ObjectPoolOnlyConfig>;
