#pragma once

#include <cstdint>
#include <cstdlib>

// ============================================================================
// SIMD-LOB Configuration Header
// 
// This file centralizes all configurable constants for the SIMD-LOB system.
// Values can be set via environment variables at compile time or use defaults.
// ============================================================================

namespace Config {

// ============================================================================
// COMPILE-TIME CONFIGURATION HELPERS
// ============================================================================

// Compile-time configuration via preprocessor defines
// These can be set via compiler flags: -DDEFAULT_POOL_SIZE=2000000

#ifndef DEFAULT_POOL_SIZE_VALUE
#define DEFAULT_POOL_SIZE_VALUE 1000000
#endif

#ifndef MAX_PRICE_LEVELS_VALUE
#define MAX_PRICE_LEVELS_VALUE 4096
#endif

#ifndef BASE_PRICE_VALUE
#define BASE_PRICE_VALUE 50000
#endif

#ifndef MIN_PRICE_TICK_VALUE
#define MIN_PRICE_TICK_VALUE 1
#endif

#ifndef TRADE_POOL_RATIO_VALUE
#define TRADE_POOL_RATIO_VALUE 10
#endif

// ============================================================================
// POOL CONFIGURATION (Runtime configurable)
// ============================================================================

// Default object pool sizes - compile-time configurable
constexpr size_t get_default_pool_size() {
    return DEFAULT_POOL_SIZE_VALUE;
}

constexpr size_t get_trade_pool_ratio() {
    return TRADE_POOL_RATIO_VALUE;
}

// Constants for easy usage
constexpr size_t DEFAULT_POOL_SIZE_CONFIG = get_default_pool_size();
constexpr size_t TRADE_POOL_SIZE_CONFIG = DEFAULT_POOL_SIZE_CONFIG / get_trade_pool_ratio();

// ============================================================================
// PRICE LEVEL CONFIGURATION (Compile-time configurable)
// ============================================================================

// Price configuration - compile-time configurable
constexpr uint32_t get_max_price_levels() {
    return MAX_PRICE_LEVELS_VALUE;
}

constexpr uint32_t get_base_price() {
    return BASE_PRICE_VALUE;
}

constexpr uint32_t get_min_price_tick() {
    return MIN_PRICE_TICK_VALUE;
}

// Constants for easy usage (avoid macro conflicts)
constexpr uint32_t MAX_PRICE_LEVELS_CONFIG = get_max_price_levels();
constexpr uint32_t BASE_PRICE_CONFIG = get_base_price();
constexpr uint32_t MIN_PRICE_TICK_CONFIG = get_min_price_tick();

// ============================================================================
// BITSET DIRECTORY CONFIGURATION (SIMD Architecture Constants)
// ============================================================================
// These values are optimized for AVX2 SIMD operations and should NOT be changed
// unless you understand the SIMD implications

namespace BitsetConfig {
    // SIMD-optimized constants - DO NOT CHANGE without updating SIMD code
    static constexpr size_t L1_BITS = 64;        // 64-bit L1 bitset (single uint64_t)
    static constexpr size_t L2_BITS = 64;        // 64-bit L2 bitsets (single uint64_t)
    static constexpr size_t CHUNK_SIZE = 64;     // Price levels per L2 chunk
    static constexpr size_t SIMD_VEC_SIZE = 4;   // AVX2 vector size (256-bit / 64-bit = 4)
    
    // Calculated constants
    static constexpr size_t MAX_PRICE_LEVELS = L1_BITS * L2_BITS; // 4096 levels
    
    // Validation
    static_assert(MAX_PRICE_LEVELS == 4096, "BitsetDirectory optimized for 4096 price levels");
    static_assert(L1_BITS == 64 && L2_BITS == 64, "SIMD code optimized for 64-bit operations");
    static_assert(SIMD_VEC_SIZE == 4, "AVX2 SIMD code assumes 4x64-bit lanes");
}

// ============================================================================
// BENCHMARK CONFIGURATION (Runtime configurable)
// ============================================================================

#ifndef BENCHMARK_ITERATIONS_VALUE
#define BENCHMARK_ITERATIONS_VALUE 10000
#endif

#ifndef WARMUP_ITERATIONS_VALUE
#define WARMUP_ITERATIONS_VALUE 1000
#endif

constexpr size_t get_benchmark_iterations() {
    return BENCHMARK_ITERATIONS_VALUE;
}

constexpr size_t get_warmup_iterations() {
    return WARMUP_ITERATIONS_VALUE;
}

constexpr size_t BENCHMARK_ITERATIONS_CONFIG = get_benchmark_iterations();
constexpr size_t WARMUP_ITERATIONS_CONFIG = get_warmup_iterations();

// ============================================================================
// MARKET DATA CONFIGURATION (Runtime configurable)
// ============================================================================

#ifndef MAX_MARKET_DEPTH_LEVELS_VALUE
#define MAX_MARKET_DEPTH_LEVELS_VALUE 10
#endif

constexpr uint32_t get_max_market_depth_levels() {
    return MAX_MARKET_DEPTH_LEVELS_VALUE;
}

constexpr uint32_t MAX_MARKET_DEPTH_LEVELS_CONFIG = get_max_market_depth_levels();

// ============================================================================
// DEBUG AND LOGGING CONFIGURATION
// ============================================================================

#ifdef SIMD_LOB_DEBUG
constexpr bool DEBUG_ENABLED_CONFIG = true;
#else
constexpr bool DEBUG_ENABLED_CONFIG = false;
#endif

#ifdef VERBOSE_LOGGING
constexpr bool VERBOSE_LOGGING_CONFIG = true;
#else
constexpr bool VERBOSE_LOGGING_CONFIG = false;
#endif

constexpr bool is_debug_enabled() {
    return DEBUG_ENABLED_CONFIG;
}

constexpr bool is_verbose_logging_enabled() {
    return VERBOSE_LOGGING_CONFIG;
}

// ============================================================================
// VALIDATION MACROS
// ============================================================================

// Ensure MAX_PRICE_LEVELS is compatible with BitsetDirectory
static_assert(MAX_PRICE_LEVELS_CONFIG <= BitsetConfig::MAX_PRICE_LEVELS, 
              "MAX_PRICE_LEVELS cannot exceed BitsetDirectory capacity of 4096");

// Ensure reasonable pool sizes
static_assert(DEFAULT_POOL_SIZE_CONFIG > 0, "DEFAULT_POOL_SIZE must be positive");

// ============================================================================
// CONVENIENCE MACROS FOR COMMON CONFIGURATIONS
// ============================================================================

// Quick configuration presets
#define CONFIG_SMALL_BOOK()   constexpr uint32_t max_levels = 1024
#define CONFIG_MEDIUM_BOOK()  constexpr uint32_t max_levels = 2048  
#define CONFIG_LARGE_BOOK()   constexpr uint32_t max_levels = 4096

#define CONFIG_PENNY_STOCK()  constexpr uint32_t base_price = 100;   /* $1.00 */
#define CONFIG_NORMAL_STOCK() constexpr uint32_t base_price = 5000;  /* $50.00 */
#define CONFIG_HIGH_PRICE()   constexpr uint32_t base_price = 50000; /* $500.00 */

#define CONFIG_SMALL_POOL()   constexpr size_t pool_size = 10000
#define CONFIG_MEDIUM_POOL()  constexpr size_t pool_size = 100000
#define CONFIG_LARGE_POOL()   constexpr size_t pool_size = 1000000

// ============================================================================
// CONFIGURATION VALIDATION
// ============================================================================

namespace Config {
    // Compile-time validation function
    constexpr bool validate_configuration() {
        return (get_default_pool_size() > 0) &&
               (get_max_price_levels() > 0) &&
               (get_max_price_levels() <= BitsetConfig::MAX_PRICE_LEVELS) &&
               (get_min_price_tick() > 0);
    }
}

#define VALIDATE_CONFIG() \
    static_assert(Config::validate_configuration(), "Invalid configuration detected")

} // namespace Config