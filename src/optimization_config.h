#pragma once

#include "config.h"
#include <type_traits>
#include <memory>

// Forward declarations
class PriceLevel;
class Order;
class Trade;
template<typename Config> class BitsetDirectory;

namespace OptimizationConfig {
// ============================================================================
// OPTIMIZATION POLICY INTERFACE
// ============================================================================

// Base configuration template
template<bool UseSimd, bool UseObjectPooling, bool UseCacheOptimization, bool UseIntrusiveLists>
struct OptimizationPolicy {
    static constexpr bool USE_SIMD = UseSimd;
    static constexpr bool USE_OBJECT_POOLING = UseObjectPooling; 
    static constexpr bool USE_CACHE_OPTIMIZATION = UseCacheOptimization;
    static constexpr bool USE_INTRUSIVE_LISTS = UseIntrusiveLists;
    
    // Memory alignment configuration based on cache optimization setting
    static constexpr size_t CACHE_LINE_SIZE = UseCacheOptimization ? 64 : alignof(max_align_t);
    static constexpr size_t MEMORY_ALIGNMENT = UseCacheOptimization ? 64 : alignof(max_align_t);
    
    // Pool size configuration
    static constexpr size_t DEFAULT_POOL_SIZE = UseObjectPooling ? 
        Config::DEFAULT_POOL_SIZE_CONFIG : 0;
    static constexpr size_t TRADE_POOL_SIZE = UseObjectPooling ? 
        Config::TRADE_POOL_SIZE_CONFIG : 0;
};

// ============================================================================
// PREDEFINED OPTIMIZATION CONFIGURATIONS
// ============================================================================

// Full optimization - all features enabled
using FullyOptimizedConfig = OptimizationPolicy<true, true, true, true>;

// Scalar baseline - no optimizations
using ScalarBaselineConfig = OptimizationPolicy<false, false, false, false>;

// SIMD only - just vectorized operations
using SimdOnlyConfig = OptimizationPolicy<true, false, false, false>;

// Memory optimized - pooling and cache optimization without SIMD
using MemoryOptimizedConfig = OptimizationPolicy<false, true, true, true>;

// Cache optimized - data structure layout optimization without pooling
using CacheOptimizedConfig = OptimizationPolicy<false, false, true, false>;

// Object pool only - memory pooling without other optimizations
using ObjectPoolOnlyConfig = OptimizationPolicy<false, true, false, false>;

// Object pool + SIMD - combination of the two best performing optimizations
using ObjectPoolSimdConfig = OptimizationPolicy<true, true, false, false>;

// ============================================================================
// CONFIGURATION SELECTION BASED ON CMAKE OPTIONS
// ============================================================================

// This will be specialized based on CMake configuration
template<typename Config>
struct ConfigurationTraits {
    using PriceLevelContainer = PriceLevel; // Default implementation
    using BitsetDirectoryType = BitsetDirectory<Config>;
    
    // Memory allocation strategy
    template<typename T>
    using AllocatorType = std::conditional_t<Config::USE_OBJECT_POOLING,
        void, // Will be replaced with ObjectPoolAllocator<T>
        std::allocator<T>
    >;
    
    // Container alignment
    template<typename T>
    static constexpr size_t get_alignment() {
        return Config::USE_CACHE_OPTIMIZATION ? Config::CACHE_LINE_SIZE : alignof(T);
    }
};

// ============================================================================
// SIMD CONFIGURATION TRAITS  
// ============================================================================

template<typename Config>
struct SimdTraits {
    static constexpr bool has_simd_support() {
        return Config::USE_SIMD;
    }
    
    // SIMD instruction set selection
    static constexpr bool use_avx2() {
#ifdef __AVX2__
        return Config::USE_SIMD;
#else
        return false;
#endif
    }
    
    static constexpr bool use_avx512() {
#ifdef __AVX512F__
        return Config::USE_SIMD;
#else
        return false;
#endif
    }
    
    // Vector size configuration
    static constexpr size_t vector_width() {
        if constexpr (use_avx512()) {
            return 8; // 512-bit / 64-bit = 8 elements
        } else if constexpr (use_avx2()) {
            return 4; // 256-bit / 64-bit = 4 elements
        } else {
            return 1; // Scalar fallback
        }
    }
};

// ============================================================================
// MEMORY OPTIMIZATION TRAITS
// ============================================================================

template<typename Config>
struct MemoryTraits {
    static constexpr bool use_object_pooling() {
        return Config::USE_OBJECT_POOLING;
    }
    
    static constexpr bool use_cache_optimization() {
        return Config::USE_CACHE_OPTIMIZATION;
    }
    
    static constexpr bool use_intrusive_lists() {
        return Config::USE_INTRUSIVE_LISTS;
    }
    
    static constexpr size_t get_cache_line_size() {
        return Config::CACHE_LINE_SIZE;
    }
    
    static constexpr size_t get_memory_alignment() {
        return Config::MEMORY_ALIGNMENT;
    }
    
    // Prefetching hints
    static constexpr bool use_prefetching() {
        return Config::USE_CACHE_OPTIMIZATION;
    }
};

// ============================================================================
// RUNTIME CONFIGURATION SELECTION
// ============================================================================

// CMake will generate this based on build configuration
#ifdef ENABLE_SIMD
    #ifdef ENABLE_OBJECT_POOLING 
        #ifdef ENABLE_CACHE_OPTIMIZATION
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = FullyOptimizedConfig;
            #else
                using DefaultConfig = OptimizationPolicy<true, true, true, false>;
            #endif
        #else
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = OptimizationPolicy<true, true, false, true>;
            #else
                using DefaultConfig = ObjectPoolSimdConfig;
            #endif
        #endif
    #else
        #ifdef ENABLE_CACHE_OPTIMIZATION
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = OptimizationPolicy<true, false, true, true>;
            #else
                using DefaultConfig = CacheOptimizedConfig;
            #endif
        #else
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = OptimizationPolicy<true, false, false, true>;
            #else
                using DefaultConfig = SimdOnlyConfig;
            #endif
        #endif
    #endif
#else
    #ifdef ENABLE_OBJECT_POOLING
        #ifdef ENABLE_CACHE_OPTIMIZATION
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = MemoryOptimizedConfig;
            #else
                using DefaultConfig = OptimizationPolicy<false, true, true, false>;
            #endif
        #else
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = OptimizationPolicy<false, true, false, true>;
            #else
                using DefaultConfig = ObjectPoolOnlyConfig;
            #endif
        #endif
    #else
        #ifdef ENABLE_CACHE_OPTIMIZATION
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = OptimizationPolicy<false, false, true, true>;
            #else
                using DefaultConfig = CacheOptimizedConfig;
            #endif
        #else
            #ifdef ENABLE_INTRUSIVE_LISTS
                using DefaultConfig = OptimizationPolicy<false, false, false, true>;
            #else
                using DefaultConfig = ScalarBaselineConfig;
            #endif
        #endif
    #endif
#endif

// ============================================================================
// COMPILE-TIME VALIDATION
// ============================================================================

template<typename Config>
constexpr bool validate_optimization_config() {
    // Ensure SIMD is only enabled if hardware supports it
    if constexpr (Config::USE_SIMD) {
        #ifndef __AVX2__
            return false; // SIMD requested but AVX2 not available
        #endif
    }
    
    // Ensure object pooling has valid pool sizes
    if constexpr (Config::USE_OBJECT_POOLING) {
        if (Config::DEFAULT_POOL_SIZE == 0) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// CONFIGURATION DEBUG HELPERS
// ============================================================================

template<typename Config>
struct ConfigDebugInfo {
    static void print_config() {
        #if defined(SIMD_LOB_DEBUG) || defined(VERBOSE_LOGGING)
        printf("Optimization Configuration:\n");
        printf("  SIMD: %s\n", Config::USE_SIMD ? "ENABLED" : "DISABLED");
        printf("  Object Pooling: %s\n", Config::USE_OBJECT_POOLING ? "ENABLED" : "DISABLED");
        printf("  Cache Optimization: %s\n", Config::USE_CACHE_OPTIMIZATION ? "ENABLED" : "DISABLED");
        printf("  Intrusive Lists: %s\n", Config::USE_INTRUSIVE_LISTS ? "ENABLED" : "DISABLED");
        printf("  Memory Alignment: %zu bytes\n", Config::MEMORY_ALIGNMENT);
        printf("  Pool Size: %zu\n", Config::DEFAULT_POOL_SIZE);
        #endif
    }
    
    static constexpr const char* get_config_name() {
        if constexpr (std::is_same_v<Config, FullyOptimizedConfig>) {
            return "FullyOptimized";
        } else if constexpr (std::is_same_v<Config, ScalarBaselineConfig>) {
            return "ScalarBaseline";
        } else if constexpr (std::is_same_v<Config, SimdOnlyConfig>) {
            return "SimdOnly";
        } else if constexpr (std::is_same_v<Config, MemoryOptimizedConfig>) {
            return "MemoryOptimized";
        } else if constexpr (std::is_same_v<Config, CacheOptimizedConfig>) {
            return "CacheOptimized";
        } else if constexpr (std::is_same_v<Config, ObjectPoolOnlyConfig>) {
            return "ObjectPoolOnly";
        } else if constexpr (std::is_same_v<Config, ObjectPoolSimdConfig>) {
            return "ObjectPoolSimd";
        } else {
            return "Custom";
        }
    }
};

} // namespace OptimizationConfig

// Validation macro
#define VALIDATE_OPTIMIZATION_CONFIG(Config) \
    static_assert(OptimizationConfig::validate_optimization_config<Config>(), \
                  "Invalid optimization configuration detected")
