#include "price_level.h"

PriceLevel::PriceLevel() : head_order(nullptr), tail_order(nullptr), price(0), total_quantity(0), order_count(0) {}

void PriceLevel::add_order(Order* order) {
    if (!order) return;
    
    order->prev = tail_order;
    order->next = nullptr;
    
    if (tail_order) {
        tail_order->next = order;
    } else {
        head_order = order;
    }
    tail_order = order;
    
    total_quantity += order->quantity;
    order_count++;
}

void PriceLevel::remove_order(Order* order) {
    if (!order) return;
    
    if (order->prev) {
        order->prev->next = order->next;
    } else {
        head_order = order->next;
    }
    
    if (order->next) {
        order->next->prev = order->prev;
    } else {
        tail_order = order->prev;
    }
    
    total_quantity -= order->quantity;
    order_count--;
}

uint32_t PriceLevel::execute_orders(uint32_t quantity, std::vector<Trade>& trades, uint64_t timestamp) {
    uint32_t filled = 0;
    
    while (head_order && filled < quantity) {
        uint32_t fill_qty = std::min(quantity - filled, head_order->quantity);
        
        Trade trade(0, head_order->order_id, price, fill_qty, timestamp);
        trades.push_back(trade);
        
        head_order->quantity -= fill_qty;
        total_quantity -= fill_qty;
        filled += fill_qty;
        
        if (head_order->quantity == 0) {
            Order* completed_order = head_order;
            head_order = head_order->next;
            if (head_order) {
                head_order->prev = nullptr;
            } else {
                tail_order = nullptr;
            }
            order_count--;
        }
    }
    
    return filled;
}

void PriceLevel::clear() {
    head_order = nullptr;
    tail_order = nullptr;
    price = 0;
    total_quantity = 0;
    order_count = 0;
}

bool PriceLevel::validate_integrity() const {
    if (is_empty()) {
        return total_quantity == 0 && order_count == 0;
    }
    
    uint32_t calculated_quantity = 0;
    uint32_t calculated_count = 0;
    
    Order* current = head_order;
    while (current) {
        calculated_quantity += current->quantity;
        calculated_count++;
        current = current->next;
    }
    
    return calculated_quantity == total_quantity && calculated_count == order_count;
}