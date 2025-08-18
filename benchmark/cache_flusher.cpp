#include "benchmark_framework.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#ifdef __x86_64__
#include <immintrin.h>
#endif

namespace BenchmarkFramework {

void CacheFlusher::flush_cpu_caches() {
    // Allocate memory larger than L3 cache to ensure eviction
    volatile char* buffer = static_cast<volatile char*>(std::aligned_alloc(CACHE_LINE_SIZE, L3_CACHE_SIZE));
    if (!buffer) {
        std::cerr << "Warning: Failed to allocate cache flush buffer" << std::endl;
        return;
    }
    
    // Fill buffer with pseudo-random data to prevent optimization
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    // Touch every cache line to flush existing cache contents
    for (size_t i = 0; i < L3_CACHE_SIZE; i += CACHE_LINE_SIZE) {
        buffer[i] = static_cast<char>(dis(gen));
        
        // Add some computation to prevent optimization
        volatile char temp = buffer[i];
        buffer[i] = temp ^ 0x55;
    }
    
    // Memory fence to ensure all operations complete
#ifdef __x86_64__
    _mm_mfence();
#else
    __sync_synchronize();
#endif
    
    std::free(const_cast<char*>(buffer));
}

void CacheFlusher::flush_memory_hierarchy() {
    // Flush TLB by accessing large, sparse memory regions
    volatile char* tlb_buffer = static_cast<volatile char*>(std::aligned_alloc(4096, TLB_FLUSH_SIZE));
    if (!tlb_buffer) {
        std::cerr << "Warning: Failed to allocate TLB flush buffer" << std::endl;
        return;
    }
    
    // Access pages in a pattern that maximizes TLB misses
    const size_t page_size = 4096;
    const size_t num_pages = TLB_FLUSH_SIZE / page_size;
    
    for (size_t i = 0; i < num_pages; i += 64) { // Every 64th page
        size_t offset = i * page_size;
        if (offset < TLB_FLUSH_SIZE) {
            tlb_buffer[offset] = static_cast<char>(i & 0xFF);
            
            // Force a read-write cycle
            volatile char temp = tlb_buffer[offset];
            tlb_buffer[offset] = temp + 1;
        }
    }
    
    // Branch predictor pollution - create unpredictable branches
    volatile int branch_pollution = 0;
    for (int i = 0; i < 10000; ++i) {
        // Create unpredictable branch pattern
        if ((i * 17 + 23) % 7 == 0) {
            branch_pollution += i;
        } else if ((i * 13) % 11 == 0) {
            branch_pollution -= i;
        } else {
            branch_pollution ^= i;
        }
    }
    
    // Ensure compiler doesn't optimize away the computation
    if (branch_pollution == 0x12345678) {
        std::cout << "Unlikely branch taken" << std::endl;
    }
    
#ifdef __x86_64__
    _mm_mfence();
#else
    __sync_synchronize();
#endif
    
    std::free(const_cast<char*>(tlb_buffer));
}

void CacheFlusher::flush_all_caches() {
    std::cout << "Flushing CPU caches and memory hierarchy..." << std::flush;
    
    // Flush CPU caches first
    flush_cpu_caches();
    
    // Small delay to let caches settle
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Flush memory hierarchy and branch predictors
    flush_memory_hierarchy();
    
    // Final memory barrier
#ifdef __x86_64__
    _mm_mfence();
    // Additional x86-specific cache line flushing
    __builtin_ia32_pause();
#else
    __sync_synchronize();
#endif
    
    // Brief settling time
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    std::cout << " done" << std::endl;
}

}
