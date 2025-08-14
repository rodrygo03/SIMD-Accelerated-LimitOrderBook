#pragma once

#include "order.h"
#include <vector>
#include <stack>
#include <cstdint>

template<typename T>
class ObjectPool {
    private:
        std::vector<T> pool;
        std::stack<T*> available;
        size_t pool_size;

    public:
        explicit ObjectPool(size_t size) : pool_size(size) {
            pool.reserve(size);
        }

        void preallocate() {
            pool.resize(pool_size);
            for (size_t i = 0; i < pool_size; ++i) {
                available.push(&pool[i]);
            }
        }

        T* acquire() {
            if (available.empty()) {
                return new T(); // Fallback allocation
            }
            T* obj = available.top();
            available.pop();
            return obj;
        }

        void release(T* obj) {
            if (obj >= &pool[0] && obj < &pool[pool_size]) {
                available.push(obj);
            } else {
                delete obj; // Was fallback allocation
            }
        }

        void reset() {
            while (!available.empty()) {
                available.pop();
            }
            for (size_t i = 0; i < pool_size; ++i) {
                available.push(&pool[i]);
            }
        }
};

using OrderPool = ObjectPool<Order>;
using TradePool = ObjectPool<Trade>;