#include "order.h"

void Order::reset(uint64_t id, uint32_t p, uint32_t qty, Side s, OrderType t, uint64_t ts) {
    order_id = id;
    price = p;
    quantity = qty;
    remaining_qty = qty;
    side = s;
    type = t;
    timestamp = ts;
    next = nullptr;
    prev = nullptr;
}

bool Order::is_filled() const {
    return remaining_qty == 0;
}

uint32_t Order::fill(uint32_t exec_qty) {
    if (is_filled()) return 0;
    uint32_t fill_qty = exec_qty > remaining_qty ? remaining_qty : exec_qty;
    remaining_qty -= fill_qty;
    return fill_qty;
}
