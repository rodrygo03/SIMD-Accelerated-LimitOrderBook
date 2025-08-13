#pragma once
#include <vector>
#include <cstdint>

#include "order.h"

template<typename T>
class ObjectPool {
  private:
    std::vector<T> pool;
    std::vector<T*> free_list;
    size_t capacity;
    size_t allocated_count;

  public:
    explicit ObjectPool(size_t initial_capacity): capacity(initial_capacity), allocated_count(0) {
        pool.reserve(capacity);
    }
    ~ObjectPool() = default;
    
    T* acquire() {
        allocated_count++;
        if (!free_list.empty()) { // reap from free_list
            T* obj = free_list.back();
            free_list.pop_back();
            return obj;
        }
        // create space in pool
        pool.resize(allocated_count);
        return &pool.back();
    }

    void release(T* obj) {
        allocated_count--;
        free_list.push_back(obj);
    }

    void preallocate() {
        pool.resize(capacity);
        for (size_t i=0; i<capacity; i++) {
            free_list.push_back(&pool[i]);
        }
    }
    
    void reset() {
      free_list.clear();
      for (size_t i = 0; i < pool.size(); i++) {
          free_list.push_back(&pool[i]);
      }
      allocated_count = 0;
    }
    
    size_t size() const { return allocated_count; }
    size_t available() const { return free_list.size(); }
    bool empty() const { return free_list.empty(); }
};

using OrderPool = ObjectPool<Order>;
using TradePool = ObjectPool<Trade>;