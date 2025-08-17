#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <cassert>

#include "order.h"

template<typename T>
class ObjectPool {
private:
    std::unique_ptr<T[]> pool_storage;
    
    // Stack-based free list for O(1) operations
    T** free_stack;
    size_t free_top;
    
    size_t capacity;
    size_t allocated_count;
    bool is_preallocated;

public:
    explicit ObjectPool(size_t initial_capacity): capacity(initial_capacity), allocated_count(0), is_preallocated(false) {
        
        pool_storage = std::make_unique<T[]>(capacity);
        free_stack = new T*[capacity];
        
        // Initialize free stack with all objects
        preallocate();
    }
    
    ~ObjectPool() {
        delete[] free_stack;
    }
    
    // High-performance acquire - O(1) with no allocations
    T* acquire() {
        if (free_top == 0) {
            // Pool exhausted 
            // TODO: graceful handling
            assert(false && "Object pool exhausted!");
            return nullptr;
        }
        
        allocated_count++;
        free_top--;
        return free_stack[free_top];
    }

    // High-performance release - O(1) with no deallocations
    void release(T* obj) {
        if (obj == nullptr) return;
        
        // Verify the object belongs to this pool (debug builds only)
        assert(obj >= pool_storage.get() && obj < pool_storage.get() + capacity);
        assert(free_top < capacity);
        
        allocated_count--;
        free_stack[free_top] = obj;
        free_top++;
    }

    void preallocate() {
        if (is_preallocated) return;
        
        for (size_t i = 0; i < capacity; i++) {
            new (&pool_storage[i]) T();
            free_stack[i] = &pool_storage[i];
        }
        
        free_top = capacity;
        allocated_count = 0;
        is_preallocated = true;
    }
    
    void reset() {
        for (size_t i = 0; i < capacity; i++) {
            pool_storage[i].~T();
        }
        
        preallocate();
    }
    
    // Statistics and debugging
    size_t size() const { return allocated_count; }
    size_t available() const { return free_top; }
    size_t total_capacity() const { return capacity; }
    bool empty() const { return free_top == 0; }
    bool is_full() const { return allocated_count == capacity; }
    
    // Memory usage information
    size_t memory_usage_bytes() const {
        return capacity * sizeof(T) + capacity * sizeof(T*);
    }
    
    // Get utilization percentage
    double utilization() const {
        return (double)allocated_count / capacity * 100.0;
    }
};

using OrderPool = ObjectPool<Order>;
using TradePool = ObjectPool<Trade>;
