#include "price_level.h"

PriceLevel::PriceLevel() : head_order(nullptr), tail_order(nullptr), price(0), total_quantity(0), order_count(0) {}

PriceLevel::PriceLevel(uint32_t p) : head_order(nullptr), tail_order(nullptr), price(p), total_quantity(0), order_count(0) {}

void PriceLevel::add_order(Order* order) {
    if (!order) return;
    
    // Optimized singly-linked list insertion
    order->next = nullptr;
    order->prev = nullptr;  // Keep for compatibility but don't use
    
    if (head_order == nullptr) {
        head_order = order;
        tail_order = order;
    } else {
        tail_order->next = order;
        tail_order = order;
    }
    
    total_quantity += order->quantity;
    order_count++;
}

void PriceLevel::remove_order(Order* order) {
    if (!order || head_order == nullptr) return;
    
    // Optimized removal for singly-linked list
    if (head_order == order) {
        head_order = head_order->next;
        if (head_order == nullptr) tail_order = nullptr;
        total_quantity -= order->quantity;
        order_count--;
        return;
    }

    Order* curr = head_order;
    while (curr->next != nullptr) {
        if (curr->next == order) {
            curr->next = order->next;
            if (curr->next == nullptr) tail_order = curr; // Removed last order
            total_quantity -= order->quantity;
            order_count--;
            return;
        }
        curr = curr->next;
    }
}

uint32_t PriceLevel::execute_orders(uint32_t quantity, std::vector<Trade>& trades, uint64_t timestamp) {
    if (head_order == nullptr) return 0;

    uint32_t total_executed = 0;

    while (head_order != nullptr && quantity > 0) {
        uint32_t exec_qty = head_order->fill(quantity);
        total_executed += exec_qty;
        quantity -= exec_qty;
        total_quantity -= exec_qty;

        if (exec_qty > 0) {
            trades.emplace_back(head_order->order_id, head_order->order_id, price, exec_qty, timestamp);
        }

        if (head_order->is_filled()) {
            // Remove completed order from head
            head_order = head_order->next;
            if (head_order == nullptr) {
                tail_order = nullptr;
            }
            order_count--;
        }
    }

    return total_executed;
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