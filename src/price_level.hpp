#pragma once

#include <cstdint>
#include <vector>

#include "order.h"

class PriceLevel {    // intrusive queue
  private:
    Order* head;        
    Order* tail;        
    uint32_t price;       // Price for this level
    uint32_t total_qty;   // Total quantity at this price level
    uint32_t order_count; // Number of orders at this level
    
  public:
    PriceLevel(): head(nullptr), tail(nullptr), price(0), total_qty(0), order_count(0) {}
    explicit PriceLevel(uint32_t p): head(nullptr), tail(nullptr), price(p), total_qty(0), order_count(0) {}
    
    void add_order(Order* order) {
        if (head == nullptr) {
            head = order;
            tail = order;
        }
        else {
            tail->next = order;
            tail = order;
        }
    }

    void remove_order(Order* order) {
        if (head == nullptr) return; 

        if (head == order) {
            head = head->next;
            if (head == nullptr) tail = nullptr;
            return;
        }

        Order* curr = head;
        while (curr->next != nullptr) {
            if (curr->next == order) {
                curr->next = order->next;
                if (curr->next == nullptr) tail = curr; // Removed last order
                return;
            }
            curr = curr->next;
        }
    }

    Order* get_front_order() const { return head; }
    
    uint32_t execute_orders(uint32_t qty, std::vector<Trade>& trades, uint64_t timestamp) {
        if (head == nullptr) return 0; // No orders to execute

        uint32_t total_executed = 0;
        Order* curr = head;

        while (curr != nullptr && qty > 0) {
            uint32_t exec_qty = curr->fill(qty);
            total_executed += exec_qty;
            qty -= exec_qty;

            if (exec_qty > 0) {
                trades.emplace_back(curr->order_id, curr->order_id, price, exec_qty, timestamp);
            }

            if (curr->is_filled()) {
                Order* to_remove = curr;
                curr = curr->next; 
                remove_order(to_remove);
                order_count--;
                total_qty -= to_remove->quantity;
            } else {
                curr = curr->next;
            }
        }

        return total_executed;
    }
    
    bool has_orders() const { return head != nullptr; }
    bool is_empty() const { return order_count == 0; }
    
    uint32_t get_price() const { return price; }
    uint32_t get_total_quantity() const { return total_qty; }
    uint32_t get_order_count() const { return order_count; }
    
    void set_price(uint32_t p) { price = p; }
    
    void clear() {
        head = nullptr;
        tail = nullptr;
        total_qty = 0;
        order_count = 0;
    }
    
    bool validate_integrity() const {
        if (head == nullptr && tail == nullptr) return true; 
        if (head == nullptr || tail == nullptr) return false; 

        Order* curr = head;
        while (curr != nullptr) {
            if (curr->next == nullptr && curr != tail) return false; // Tail must be last
            curr = curr->next;
        }
        
        return true;
    }
};