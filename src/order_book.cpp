#include <algorithm>
#include <emmintrin.h>

#include "order_book.h"

template<typename Config>
OrderBook<Config>::OrderBook(size_t initial_pool_size): 
  buy_directory(), sell_directory(),
  order_pool(Config::USE_OBJECT_POOLING ? initial_pool_size : 1),
  trade_pool(Config::USE_OBJECT_POOLING ? initial_pool_size / ::Config::get_trade_pool_ratio() : 1),
  cached_best_bid_idx(0),
  cached_best_ask_idx(0),
  best_bid_valid(false),
  best_ask_valid(false),
  total_orders_processed(0),
  total_trades_executed(0),
  total_volume_traded(0) {

    // Initialize cache-aligned price level arrays
    for (uint32_t i=0; i<MAX_PRICE_LEVELS; i++) {
        buy_levels[i] = PriceLevel();
        sell_levels[i] = PriceLevel();
    }

    if constexpr (Config::USE_OBJECT_POOLING) {
        order_pool.preallocate();
        trade_pool.preallocate();
    }
    // Reserve hash map capacity to prevent expensive rehashing
    order_map.reserve(initial_pool_size);
}

template<typename Config>
bool OrderBook<Config>::add_limit_order(uint64_t order_id, Side side, uint32_t price, uint32_t quantity, uint64_t timestamp) {
    if (quantity == 0 || order_map.find(order_id) != order_map.end()) {
        return false;
    }

    Order* order;
    if constexpr (Config::USE_OBJECT_POOLING) {
        order = order_pool.acquire();
    } else {
        order = new Order();
    }
    order->reset(order_id, price, quantity, side, OrderType::LIMIT, timestamp);
    order_map[order_id] = order;

    uint32_t level_idx;
    if (side == Side::BUY) {
        level_idx = price_to_buy_index(price);
        buy_levels[level_idx].add_order(order);
        buy_levels[level_idx].set_price(price);
        buy_directory.set_bit(level_idx);
    } 
    else {
        level_idx = price_to_sell_index(price);
        sell_levels[level_idx].add_order(order);
        sell_levels[level_idx].set_price(price);
        sell_directory.set_bit(level_idx);
    }

    invalidate_best_prices();
    total_orders_processed++;
    return true;
}

template<typename Config>
bool OrderBook<Config>::cancel_order(uint64_t order_id) {
    auto it = order_map.find(order_id);
    if (it == order_map.end()) {
        return false;
    }

    Order* order = it->second;
    remove_order_from_level(order, order->side);
    order_map.erase(it);
    
    if constexpr (Config::USE_OBJECT_POOLING) {
        order_pool.release(order);
    } 
    else {
        delete order;
    }
    invalidate_best_prices();
    return true;
}

template<typename Config>
bool OrderBook<Config>::modify_order(uint64_t order_id, uint32_t new_price, uint32_t new_quantity, uint64_t timestamp) {
    // Cancel-replace semantics 
    auto it = order_map.find(order_id);
    if (it == order_map.end() || new_quantity == 0) {
        return false;
    }

    Order* order = it->second;
    Side side = order->side;
    
    bool cancel_success = cancel_order(order_id);
    if (!cancel_success) {
        return false;
    }
    return add_limit_order(order_id, side, new_price, new_quantity, timestamp);
}

template<typename Config>
uint32_t OrderBook<Config>::execute_market_order(Side side, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
    uint32_t remaining_qty = quantity;
    uint32_t total_filled = 0;

    if (side == Side::BUY) {
        // Buy market order executes against sell levels (ascending price)
        uint32_t start_idx = sell_directory.find_lowest_bit();
        while (remaining_qty>0 && start_idx<MAX_PRICE_LEVELS) {
            PriceLevel& level = sell_levels[start_idx];
            if (level.has_orders()) {
                uint32_t filled = level.execute_orders(remaining_qty, trades, timestamp);
                total_filled += filled;
                remaining_qty -= filled;
                total_volume_traded += filled;
                
                if (level.is_empty()) {
                    sell_directory.clear_bit(start_idx);
                }
            }
            start_idx = sell_directory.find_next_higher_bit(start_idx);
        }
    } else {
        // Sell market order executes against buy levels (highest price first)
        uint32_t start_idx = buy_directory.find_lowest_bit(); // Start from lowest index (highest price)
        while (remaining_qty>0 && start_idx<MAX_PRICE_LEVELS) {
            PriceLevel& level = buy_levels[start_idx];
            if (level.has_orders()) {
                uint32_t filled = level.execute_orders(remaining_qty, trades, timestamp);
                total_filled += filled;
                remaining_qty -= filled;
                total_volume_traded += filled;
                
                if (level.is_empty()) {
                    buy_directory.clear_bit(start_idx);
                }
            }
            start_idx = buy_directory.find_next_higher_bit(start_idx); // Move to lower price
        }
    }

    if (total_filled > 0) {
        total_trades_executed++;
        invalidate_best_prices();
    }
    
    return total_filled;
}

template<typename Config>
uint32_t OrderBook<Config>::execute_ioc_order(Side side, uint32_t price, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
    uint32_t remaining_qty = quantity;
    uint32_t total_filled = 0;

    if (side == Side::BUY) {
        // IOC buy executes against sells at or below the limit price
        uint32_t start_idx = sell_directory.find_lowest_bit();
        while (remaining_qty>0 && start_idx<MAX_PRICE_LEVELS) {
            uint32_t level_price = sell_index_to_price(start_idx);
            if (level_price > price) break; // Price too high
            
            PriceLevel& level = sell_levels[start_idx];
            if (level.has_orders()) {
                uint32_t filled = level.execute_orders(remaining_qty, trades, timestamp);
                total_filled += filled;
                remaining_qty -= filled;
                total_volume_traded += filled;
                
                if (level.is_empty()) {
                    sell_directory.clear_bit(start_idx);
                }
            }
            start_idx = sell_directory.find_next_higher_bit(start_idx);
        }
    } 
    else {
        // IOC sell executes against buys at or above the limit price  
        uint32_t start_idx = buy_directory.find_lowest_bit(); // Start from best bid (highest price)
        while (remaining_qty > 0 && start_idx < MAX_PRICE_LEVELS) {
            uint32_t level_price = buy_index_to_price(start_idx);
            // IOC logic: sell executes against buys at or above the limit price
            if (level_price < price) break; // Price too low
            
            PriceLevel& level = buy_levels[start_idx];
            if (level.has_orders()) {
                uint32_t filled = level.execute_orders(remaining_qty, trades, timestamp);
                total_filled += filled;
                remaining_qty -= filled;
                total_volume_traded += filled;
                
                if (level.is_empty()) {
                    buy_directory.clear_bit(start_idx);
                }
            }
            start_idx = buy_directory.find_next_higher_bit(start_idx); // Move to lower price
        }
    }

    if (total_filled > 0) {
        total_trades_executed++;
        invalidate_best_prices();
    }
    
    return total_filled;
}

template<typename Config>
uint32_t OrderBook<Config>::get_best_bid() const {
    // Only recompute when cache is invalid
    if (!best_bid_valid) {
        refresh_best_bid_cache();
    }
    
    if (cached_best_bid_idx >= MAX_PRICE_LEVELS) {
        return 0; // No bids
    }
    
    // Cache hit: Return cached result
    return buy_index_to_price(cached_best_bid_idx);
}

template<typename Config>
uint32_t OrderBook<Config>::get_best_ask() const {
    if (!best_ask_valid) {
        refresh_best_ask_cache();
    }
    
    if (cached_best_ask_idx >= MAX_PRICE_LEVELS) {
        return UINT32_MAX; // No asks
    }
    
    return sell_index_to_price(cached_best_ask_idx);
}

template<typename Config>
uint32_t OrderBook<Config>::get_best_bid_quantity() const {
    // Reuse cached best bid index
    if (!best_bid_valid) {
        refresh_best_bid_cache();
    }
    
    if (cached_best_bid_idx >= MAX_PRICE_LEVELS) {
        return 0;
    }
    
    // Direct array access using cached index
    return buy_levels[cached_best_bid_idx].get_total_quantity();
}

template<typename Config>
uint32_t OrderBook<Config>::get_best_ask_quantity() const {
    // Reuse cached best ask index
    if (!best_ask_valid) {
        refresh_best_ask_cache();
    }
    
    if (cached_best_ask_idx >= MAX_PRICE_LEVELS) {
        return 0;
    }
    
    // Direct array access using cached index
    return sell_levels[cached_best_ask_idx].get_total_quantity();
}

template<typename Config>
bool OrderBook<Config>::is_crossed() const {
    uint32_t best_bid = get_best_bid();
    uint32_t best_ask = get_best_ask();
    
    return (best_bid != 0 && best_ask != UINT32_MAX && best_bid >= best_ask);
}

template<typename Config>
const PriceLevel* OrderBook<Config>::get_price_level(Side side, uint32_t price) const {
    if (side == Side::BUY) {
        uint32_t idx = price_to_buy_index(price);
        return &buy_levels[idx];
    } else {
        uint32_t idx = price_to_sell_index(price);
        return &sell_levels[idx];
    }
}

template<typename Config>
void OrderBook<Config>::get_market_depth(uint32_t levels, 
  std::vector<std::pair<uint32_t, uint32_t>>& bids,
  std::vector<std::pair<uint32_t, uint32_t>>& asks) const {
    bids.clear();
    asks.clear();
    bids.reserve(levels);
    asks.reserve(levels);

    // Get bid levels (highest to lowest price)
    uint32_t bid_idx = buy_directory.find_lowest_bit(); // Start from lowest index (highest price)
    uint32_t bid_count = 0;
    while (bid_idx < MAX_PRICE_LEVELS && bid_count < levels) {
        const PriceLevel& level = buy_levels[bid_idx];
        if (level.has_orders()) {
            bids.emplace_back(level.get_price(), level.get_total_quantity());
            bid_count++;
        }
        bid_idx = buy_directory.find_next_higher_bit(bid_idx); // Move to higher index (lower price)
    }

    // Get ask levels (lowest to highest)
    uint32_t ask_idx = sell_directory.find_lowest_bit();
    uint32_t ask_count = 0;
    while (ask_idx < MAX_PRICE_LEVELS && ask_count < levels) {
        const PriceLevel& level = sell_levels[ask_idx];
        if (level.has_orders()) {
            asks.emplace_back(level.get_price(), level.get_total_quantity());
            ask_count++;
        }
        ask_idx = sell_directory.find_next_higher_bit(ask_idx);
    }
}

template<typename Config>
void OrderBook<Config>::clear() {
    for (uint32_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
        buy_levels[i].clear();
        sell_levels[i].clear();
    }
    
    buy_directory.clear_all();
    sell_directory.clear_all();
    order_map.clear();
    
    // Conditionally reset object pools
    if constexpr (Config::USE_OBJECT_POOLING) {
        order_pool.reset();
        trade_pool.reset();
    }
    
    invalidate_best_prices();
    reset_statistics();
}

template<typename Config>
bool OrderBook<Config>::validate_integrity() const {
    // Validate bitset directory consistency
    if (!buy_directory.validate_consistency() || !sell_directory.validate_consistency()) {
        return false;
    }

    // Validate price level integrity
    for (uint32_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
        if (!buy_levels[i].validate_integrity() || !sell_levels[i].validate_integrity()) {
            return false;
        }
        
        // Verify directory bits match level state
        bool buy_has_orders = buy_levels[i].has_orders();
        bool buy_bit_set = buy_directory.test_bit(i);
        if (buy_has_orders != buy_bit_set) {
            return false;
        }
        
        bool sell_has_orders = sell_levels[i].has_orders();
        bool sell_bit_set = sell_directory.test_bit(i);
        if (sell_has_orders != sell_bit_set) {
            return false;
        }
    }

    return true;
}

template<typename Config>
void OrderBook<Config>::reset_statistics() {
    total_orders_processed = 0;
    total_trades_executed = 0;
    total_volume_traded = 0;
}

// Price conversion methods - implement ladder mapping
template<typename Config>
uint32_t OrderBook<Config>::price_to_buy_index(uint32_t price) const {
    // SEGFAULT FIX: was returning idx 4096 for price 50000, writing out of bounds
    // now properly maps prices around BASE_PRICE with boundary checks
    if (price > BASE_PRICE + (MAX_PRICE_LEVELS/2 - 1) * MIN_PRICE_TICK) return 0;
    if (price < BASE_PRICE - (MAX_PRICE_LEVELS/2) * MIN_PRICE_TICK) return MAX_PRICE_LEVELS - 1;
    return (BASE_PRICE + (MAX_PRICE_LEVELS/2 - 1) * MIN_PRICE_TICK - price) / MIN_PRICE_TICK;
}

template<typename Config>
uint32_t OrderBook<Config>::price_to_sell_index(uint32_t price) const {
    // Lower prices get lower indices (best ask at index 0)
    if (price < BASE_PRICE) return 0;
    if (price >= BASE_PRICE + MAX_PRICE_LEVELS * MIN_PRICE_TICK) return MAX_PRICE_LEVELS - 1;
    return (price - BASE_PRICE) / MIN_PRICE_TICK;
}

template<typename Config>
uint32_t OrderBook<Config>::buy_index_to_price(uint32_t index) const {
    if (index >= MAX_PRICE_LEVELS) return 0;
    return BASE_PRICE + (MAX_PRICE_LEVELS/2 - 1) * MIN_PRICE_TICK - (index * MIN_PRICE_TICK);
}

template<typename Config>
uint32_t OrderBook<Config>::sell_index_to_price(uint32_t index) const {
    if (index >= MAX_PRICE_LEVELS) return UINT32_MAX;
    return BASE_PRICE + (index * MIN_PRICE_TICK);
}

template<typename Config>
void OrderBook<Config>::invalidate_best_prices() {
    // Mark cached values as stale for lazy recomputation
    best_bid_valid = false;
    best_ask_valid = false;
}

template<typename Config>
void OrderBook<Config>::refresh_best_bid_cache() const {
    // BID/ASK DIRECTION FIX: was using find_highest_bit() which is wrong!
    // for buys: higher prices = lower indices, so need find_lowest_bit() for best bid
    cached_best_bid_idx = buy_directory.find_lowest_bit();
    best_bid_valid = true;
}

template<typename Config>
void OrderBook<Config>::refresh_best_ask_cache() const {
    // Compute and cache best ask index using SIMD bitset
    cached_best_ask_idx = sell_directory.find_lowest_bit();
    best_ask_valid = true;
}

template<typename Config>
void OrderBook<Config>::execute_trade(Order* buy_order, Order* sell_order, uint32_t price,
  uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
    Trade* trade;
    if constexpr (Config::USE_OBJECT_POOLING) {
        trade = trade_pool.acquire();
        *trade = Trade(buy_order->order_id, sell_order->order_id, price, quantity, timestamp);
        trades.push_back(*trade);
        trade_pool.release(trade);
    } else {
        trades.emplace_back(buy_order->order_id, sell_order->order_id, price, quantity, timestamp);
    }
}

template<typename Config>
void OrderBook<Config>::remove_order_from_level(Order* order, Side side) {
    uint32_t level_idx;
    if (side == Side::BUY) {
        level_idx = price_to_buy_index(order->price);
        buy_levels[level_idx].remove_order(order);
        if (buy_levels[level_idx].is_empty()) {
            buy_directory.clear_bit(level_idx);
        }
    } else {
        level_idx = price_to_sell_index(order->price);
        sell_levels[level_idx].remove_order(order);
        if (sell_levels[level_idx].is_empty()) {
            sell_directory.clear_bit(level_idx);
        }
    }
}

template<typename Config>
void OrderBook<Config>::prefetch_price_levels(Side side, uint32_t start_index, uint32_t count) const {
    // Prefetch price levels into L1 cache for faster access (only if cache optimization enabled)
    if constexpr (Config::USE_CACHE_OPTIMIZATION) {
        if (side == Side::BUY) {
            for (uint32_t i = 0; i < count && (start_index + i) < MAX_PRICE_LEVELS; ++i) {
                _mm_prefetch(&buy_levels[start_index + i], _MM_HINT_T0);
            }
        } else {
            for (uint32_t i = 0; i < count && (start_index + i) < MAX_PRICE_LEVELS; ++i) {
                _mm_prefetch(&sell_levels[start_index + i], _MM_HINT_T0);
            }
        }
    }
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

// Instantiate templates for all supported configurations
template class OrderBook<OptimizationConfig::FullyOptimizedConfig>;
template class OrderBook<OptimizationConfig::ScalarBaselineConfig>;  
template class OrderBook<OptimizationConfig::SimdOnlyConfig>;
template class OrderBook<OptimizationConfig::MemoryOptimizedConfig>;
template class OrderBook<OptimizationConfig::CacheOptimizedConfig>;
template class OrderBook<OptimizationConfig::ObjectPoolOnlyConfig>;
template class OrderBook<OptimizationConfig::ObjectPoolSimdConfig>;
