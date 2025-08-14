#include "scalar_order_book.h"
#include <algorithm>

ScalarOrderBook::ScalarOrderBook(): total_orders_processed(0), total_trades_executed(0),
  total_volume_traded(0) {}
    // No pre-allocation optimizations - relies on STL container growth
    // No object pool initialization - each order allocated individually
    // No cache-aligned array setup - scattered heap allocation}

ScalarOrderBook::~ScalarOrderBook() {
    // Clean up all dynamically allocated orders
    for (auto& [price, orders] : buy_levels) {
        for (Order* order : orders) {
            delete order;  // Manual deletion vs object pool recycling
        }
    }
    for (auto& [price, orders] : sell_levels) {
        for (Order* order : orders) {
            delete order;  // Manual deletion vs object pool recycling
        }
    }
}

bool ScalarOrderBook::add_limit_order(uint64_t order_id, Side side, uint32_t price, 
  uint32_t quantity, uint64_t timestamp) {
    if (quantity == 0 || order_map.find(order_id) != order_map.end()) {
        return false;
    }

    // Heap allocation for each order - no object pooling optimization
    Order* order = new Order(order_id, price, quantity, side, OrderType::LIMIT, timestamp);
    order_map[order_id] = order;

    if (side == Side::BUY) {
        // std::map automatically maintains sorted order - O(log n) insertion
        buy_levels[price].push_back(order);  
    } else {
        sell_levels[price].push_back(order);
    }

    total_orders_processed++;
    return true;
}

bool ScalarOrderBook::cancel_order(uint64_t order_id) {
    auto it = order_map.find(order_id);
    if (it == order_map.end()) {
        return false;
    }

    Order* order = it->second;
    remove_order_from_level(order, order->side);
    order_map.erase(it);
    delete order;  // Manual memory management vs object pool release
    return true;
}

bool ScalarOrderBook::modify_order(uint64_t order_id, uint32_t new_price, 
  uint32_t new_quantity, uint64_t timestamp) {
    // Cancel-replace semantics - same as optimized version
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

uint32_t ScalarOrderBook::execute_market_order(Side side, uint32_t quantity, 
  uint64_t timestamp, std::vector<Trade>& trades) {
    uint32_t remaining_qty = quantity;
    uint32_t total_filled = 0;

    if (side == Side::BUY) {
        // Buy market order executes against sell levels (ascending price)
        // std::map iteration is O(n) vs O(1) bitset directory scan
        auto it = sell_levels.begin();
        while (it != sell_levels.end() && remaining_qty > 0) {
            uint32_t filled = execute_orders_at_level(it->second, remaining_qty, 
                                                     it->first, timestamp, trades);
            total_filled += filled;
            remaining_qty -= filled;
            
            if (it->second.empty()) {
                it = sell_levels.erase(it);  // Remove empty price level
            } 
            else {
                ++it;
            }
        }
    } 
    else {
        // Sell market order executes against buy levels (descending price)  
        // Reverse iteration through std::map - O(n) traversal
        auto it = buy_levels.rbegin();
        while (it != buy_levels.rend() && remaining_qty > 0) {
            uint32_t filled = execute_orders_at_level(it->second, remaining_qty,
                                                     it->first, timestamp, trades);
            total_filled += filled;
            remaining_qty -= filled;
            
            if (it->second.empty()) {
                // Erase from reverse iterator is complex - convert to forward iterator
                auto forward_it = std::next(it).base();
                buy_levels.erase(forward_it);
            } 
            else {
                ++it;
            }
        }
    }

    total_trades_executed += trades.size();
    total_volume_traded += total_filled;
    return total_filled;
}

uint32_t ScalarOrderBook::execute_ioc_order(Side side, uint32_t price, uint32_t quantity,
                                           uint64_t timestamp, std::vector<Trade>& trades) {
    uint32_t remaining_qty = quantity;
    uint32_t total_filled = 0;

    if (side == Side::BUY) {
        // IOC buy order executes against sell levels at or below price
        auto it = sell_levels.begin();
        while (it != sell_levels.end() && it->first <= price && remaining_qty > 0) {
            uint32_t filled = execute_orders_at_level(it->second, remaining_qty,
                                                     it->first, timestamp, trades);
            total_filled += filled;
            remaining_qty -= filled;
            
            if (it->second.empty()) {
                it = sell_levels.erase(it);
            } 
            else {
                ++it;
            }
        }
    } 
    else {
        // IOC sell order executes against buy levels at or above price
        auto it = buy_levels.lower_bound(price);
        while (it != buy_levels.end() && remaining_qty > 0) {
            uint32_t filled = execute_orders_at_level(it->second, remaining_qty,
                                                     it->first, timestamp, trades);
            total_filled += filled;
            remaining_qty -= filled;
            
            if (it->second.empty()) {
                it = buy_levels.erase(it);
            } 
            else {
                ++it;
            }
        }
    }

    total_trades_executed += trades.size();
    total_volume_traded += total_filled;
    return total_filled;
}

uint32_t ScalarOrderBook::get_best_bid() const {
    // O(log n) operation - reverse iterator to get highest price
    // vs O(1) bitset directory lookup in optimized version
    return buy_levels.empty() ? 0 : buy_levels.rbegin()->first;
}

uint32_t ScalarOrderBook::get_best_ask() const {
    // O(log n) operation - forward iterator to get lowest price
    // vs O(1) bitset directory lookup in optimized version
    return sell_levels.empty() ? 0 : sell_levels.begin()->first;
}

uint32_t ScalarOrderBook::get_best_bid_quantity() const {
    if (buy_levels.empty()) return 0;
    
    const auto& order_queue = buy_levels.rbegin()->second;
    uint32_t total_qty = 0;
    // Linear scan through list to calculate total quantity - no cached value
    for (const Order* order : order_queue) {
        total_qty += order->remaining_qty;
    }
    return total_qty;
}

uint32_t ScalarOrderBook::get_best_ask_quantity() const {
    if (sell_levels.empty()) return 0;
    
    const auto& order_queue = sell_levels.begin()->second;
    uint32_t total_qty = 0;
    // Linear scan through list to calculate total quantity - no cached value
    for (const Order* order : order_queue) {
        total_qty += order->remaining_qty;
    }
    return total_qty;
}

bool ScalarOrderBook::is_crossed() const {
    if (buy_levels.empty() || sell_levels.empty()) return false;
    return get_best_bid() >= get_best_ask();
}

void ScalarOrderBook::get_market_depth(uint32_t levels,
  std::vector<std::pair<uint32_t, uint32_t>>& bids,
  std::vector<std::pair<uint32_t, uint32_t>>& asks) const {
    bids.clear();
    asks.clear();
    
    // Collect bid levels (descending price order)
    auto bid_it = buy_levels.rbegin();
    for (uint32_t i = 0; i < levels && bid_it != buy_levels.rend(); ++i, ++bid_it) {
        uint32_t total_qty = 0;
        for (const Order* order : bid_it->second) {
            total_qty += order->remaining_qty;
        }
        if (total_qty > 0) {
            bids.emplace_back(bid_it->first, total_qty);
        }
    }
    
    // Collect ask levels (ascending price order)
    auto ask_it = sell_levels.begin();
    for (uint32_t i = 0; i < levels && ask_it != sell_levels.end(); ++i, ++ask_it) {
        uint32_t total_qty = 0;
        for (const Order* order : ask_it->second) {
            total_qty += order->remaining_qty;
        }
        if (total_qty > 0) {
            asks.emplace_back(ask_it->first, total_qty);
        }
    }
}

void ScalarOrderBook::clear() {
    // Manual cleanup vs object pool reset
    for (auto& [price, orders] : buy_levels) {
        for (Order* order : orders) {
            delete order;
        }
    }
    for (auto& [price, orders] : sell_levels) {
        for (Order* order : orders) {
            delete order;
        }
    }
    
    buy_levels.clear();
    sell_levels.clear();
    order_map.clear();
    reset_statistics();
}

bool ScalarOrderBook::validate_integrity() const {
    // Validate that all orders in maps are also in order_map
    for (const auto& [price, orders] : buy_levels) {
        for (const Order* order : orders) {
            if (order_map.find(order->order_id) == order_map.end()) {
                return false;
            }
        }
    }
    for (const auto& [price, orders] : sell_levels) {
        for (const Order* order : orders) {
            if (order_map.find(order->order_id) == order_map.end()) {
                return false;
            }
        }
    }
    return true;
}

void ScalarOrderBook::reset_statistics() {
    total_orders_processed = 0;
    total_trades_executed = 0;
    total_volume_traded = 0;
}

void ScalarOrderBook::remove_order_from_level(Order* order, Side side) {
    uint32_t price = order->price;
    
    if (side == Side::BUY) {
        auto level_it = buy_levels.find(price);
        if (level_it != buy_levels.end()) {
            auto& order_queue = level_it->second;
            // Linear search through list to find order - O(n) vs O(1) pointer removal
            order_queue.remove(order);
            if (order_queue.empty()) {
                buy_levels.erase(level_it);  // Clean up empty price level
            }
        }
    } 
    else {
        auto level_it = sell_levels.find(price);
        if (level_it != sell_levels.end()) {
            auto& order_queue = level_it->second;
            // Linear search through list to find order - O(n) vs O(1) pointer removal
            order_queue.remove(order);
            if (order_queue.empty()) {
                sell_levels.erase(level_it);  // Clean up empty price level
            }
        }
    }
}

uint32_t ScalarOrderBook::execute_orders_at_level(std::list<Order*>& order_queue, 
  uint32_t quantity, uint32_t price, uint64_t timestamp, std::vector<Trade>& trades) {
    uint32_t total_executed = 0;
    
    auto it = order_queue.begin();
    while (it != order_queue.end() && quantity > 0) {
        Order* order = *it;
        uint32_t exec_qty = order->fill(quantity);
        total_executed += exec_qty;
        quantity -= exec_qty;
        
        if (exec_qty > 0) {
            // Create trade - uses push_back vs emplace_back (minor performance difference)
            trades.push_back(Trade(order->order_id, order->order_id, price, exec_qty, timestamp));
        }
        
        if (order->is_filled()) {
            // Remove filled order from list and delete - O(1) list removal
            it = order_queue.erase(it);
            order_map.erase(order->order_id);
            delete order;  // Manual deletion vs object pool release
        } 
        else {
            ++it;
        }
    }
    
    return total_executed;
}

void ScalarOrderBook::execute_trade(Order* buy_order, Order* sell_order, uint32_t price,
                                   uint32_t quantity, uint64_t timestamp, std::vector<Trade>& trades) {
    // Simple trade creation - same logic as optimized version
    trades.emplace_back(buy_order->order_id, sell_order->order_id, price, quantity, timestamp);
    total_trades_executed++;
    total_volume_traded += quantity;
}