#include <algorithm>
#include <emmintrin.h>

#include "order_book.h"

OrderBook::OrderBook(size_t initial_pool_size): buy_directory(), sell_directory(), order_pool(initial_pool_size),
  trade_pool(initial_pool_size / 10), // Cache optimization: fewer trades than orders typically
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

    // Pre-allocate object pools to avoid malloc/free during trading
    order_pool.preallocate();
    trade_pool.preallocate();
    // Reserve hash map capacity to prevent expensive rehashing
    order_map.reserve(initial_pool_size);
}

bool OrderBook::add_limit_order(uint64_t order_id, Side side, uint32_t price, uint32_t quantity, uint64_t timestamp) {
    if (quantity == 0 || order_map.find(order_id) != order_map.end()) {
        return false;
    }

    // Use pre-allocated object from pool (no malloc)
    Order* order = order_pool.acquire();
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

bool OrderBook::cancel_order(uint64_t order_id, uint64_t timestamp) {
    auto it = order_map.find(order_id);
    if (it == order_map.end()) {
        return false;
    }

    Order* order = it->second;
    remove_order_from_level(order, order->side);
    order_map.erase(it);
    order_pool.release(order);
    invalidate_best_prices();
    return true;
}

bool OrderBook::modify_order(uint64_t order_id, uint32_t new_price, uint32_t new_quantity, uint64_t timestamp) {
    // Cancel-replace semantics for simplicity
    auto it = order_map.find(order_id);
    if (it == order_map.end() || new_quantity == 0) {
        return false;
    }

    Order* order = it->second;
    Side side = order->side;
    
    cancel_order(order_id, timestamp);
    return add_limit_order(order_id, side, new_price, new_quantity, timestamp);
}

uint32_t OrderBook::execute_market_order(Side side, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
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
        // Sell market order executes against buy levels (descending price)
        uint32_t start_idx = buy_directory.find_highest_bit();
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
            start_idx = buy_directory.find_next_lower_bit(start_idx);
        }
    }

    if (total_filled > 0) {
        total_trades_executed++;
        invalidate_best_prices();
    }
    
    return total_filled;
}

uint32_t OrderBook::execute_ioc_order(Side side, uint32_t price, uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
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
        uint32_t start_idx = buy_directory.find_highest_bit();
        while (remaining_qty > 0 && start_idx < MAX_PRICE_LEVELS) {
            uint32_t level_price = buy_index_to_price(start_idx);
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
            start_idx = buy_directory.find_next_lower_bit(start_idx);
        }
    }

    if (total_filled > 0) {
        total_trades_executed++;
        invalidate_best_prices();
    }
    
    return total_filled;
}

uint32_t OrderBook::get_best_bid() const {
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

uint32_t OrderBook::get_best_ask() const {
    if (!best_ask_valid) {
        refresh_best_ask_cache();
    }
    
    if (cached_best_ask_idx >= MAX_PRICE_LEVELS) {
        return UINT32_MAX; // No asks
    }
    
    return sell_index_to_price(cached_best_ask_idx);
}

uint32_t OrderBook::get_best_bid_quantity() const {
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

uint32_t OrderBook::get_best_ask_quantity() const {
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

bool OrderBook::is_crossed() const {
    uint32_t best_bid = get_best_bid();
    uint32_t best_ask = get_best_ask();
    
    return (best_bid != 0 && best_ask != UINT32_MAX && best_bid >= best_ask);
}

const PriceLevel* OrderBook::get_price_level(Side side, uint32_t price) const {
    if (side == Side::BUY) {
        uint32_t idx = price_to_buy_index(price);
        return &buy_levels[idx];
    } else {
        uint32_t idx = price_to_sell_index(price);
        return &sell_levels[idx];
    }
}

void OrderBook::get_market_depth(uint32_t levels, 
  std::vector<std::pair<uint32_t, uint32_t>>& bids,
  std::vector<std::pair<uint32_t, uint32_t>>& asks) const {
    bids.clear();
    asks.clear();
    bids.reserve(levels);
    asks.reserve(levels);

    // Get bid levels (highest to lowest)
    uint32_t bid_idx = buy_directory.find_highest_bit();
    uint32_t bid_count = 0;
    while (bid_idx < MAX_PRICE_LEVELS && bid_count < levels) {
        const PriceLevel& level = buy_levels[bid_idx];
        if (level.has_orders()) {
            bids.emplace_back(level.get_price(), level.get_total_quantity());
            bid_count++;
        }
        bid_idx = buy_directory.find_next_lower_bit(bid_idx);
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

void OrderBook::clear() {
    for (uint32_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
        buy_levels[i].clear();
        sell_levels[i].clear();
    }
    
    buy_directory.clear_all();
    sell_directory.clear_all();
    order_map.clear();
    order_pool.reset();
    trade_pool.reset();
    
    invalidate_best_prices();
    reset_statistics();
}

bool OrderBook::validate_integrity() const {
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

void OrderBook::reset_statistics() {
    total_orders_processed = 0;
    total_trades_executed = 0;
    total_volume_traded = 0;
}

// Price conversion methods - implement ladder mapping
uint32_t OrderBook::price_to_buy_index(uint32_t price) const {
    // Higher prices get lower indices (best bid at index 0)
    return (BASE_PRICE + MAX_PRICE_LEVELS * MIN_PRICE_TICK - price) / MIN_PRICE_TICK;
}

uint32_t OrderBook::price_to_sell_index(uint32_t price) const {
    // Lower prices get lower indices (best ask at index 0)
    return (price - BASE_PRICE) / MIN_PRICE_TICK;
}

uint32_t OrderBook::buy_index_to_price(uint32_t index) const {
    return BASE_PRICE + MAX_PRICE_LEVELS * MIN_PRICE_TICK - (index * MIN_PRICE_TICK);
}

uint32_t OrderBook::sell_index_to_price(uint32_t index) const {
    return BASE_PRICE + (index * MIN_PRICE_TICK);
}

void OrderBook::invalidate_best_prices() {
    // Mark cached values as stale for lazy recomputation
    best_bid_valid = false;
    best_ask_valid = false;
}

void OrderBook::refresh_best_bid_cache() const {
    // Compute and cache best bid index using SIMD bitset
    cached_best_bid_idx = buy_directory.find_highest_bit();
    best_bid_valid = true;
}

void OrderBook::refresh_best_ask_cache() const {
    // Compute and cache best ask index using SIMD bitset
    cached_best_ask_idx = sell_directory.find_lowest_bit();
    best_ask_valid = true;
}

void OrderBook::execute_trade(Order* buy_order, Order* sell_order, uint32_t price,
  uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
    Trade* trade = trade_pool.acquire();
    *trade = Trade(buy_order->order_id, sell_order->order_id, price, quantity, timestamp);
    trades.push_back(*trade);
    trade_pool.release(trade);
}

void OrderBook::remove_order_from_level(Order* order, Side side) {
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

void OrderBook::prefetch_price_levels(Side side, uint32_t start_index, uint32_t count) const {
    // Prefetch price levels into L1 cache for faster access
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
