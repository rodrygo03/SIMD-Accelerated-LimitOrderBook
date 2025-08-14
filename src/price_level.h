#pragma once

#include "order.h"
#include <vector>
#include <cstdint>

class PriceLevel {
    private:
        Order* head_order;      // FIFO queue head
        Order* tail_order;      // FIFO queue tail
        uint32_t price;         // Price of this level
        uint32_t total_quantity; // Total quantity at this level
        uint32_t order_count;   // Number of orders

    public:
        PriceLevel();
        ~PriceLevel() = default;

        void add_order(Order* order);
        void remove_order(Order* order);
        uint32_t execute_orders(uint32_t quantity, std::vector<Trade>& trades, uint64_t timestamp);
        
        void clear();
        bool is_empty() const { return head_order == nullptr; }
        bool has_orders() const { return head_order != nullptr; }
        
        uint32_t get_price() const { return price; }
        void set_price(uint32_t new_price) { price = new_price; }
        uint32_t get_total_quantity() const { return total_quantity; }
        uint32_t get_order_count() const { return order_count; }
        
        bool validate_integrity() const;
};