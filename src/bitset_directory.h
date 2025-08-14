#pragma once

#include <cstdint>
#include <immintrin.h>  // AVX2/AVX-512 intrinsics
#include <type_traits>

#include "config.h"
#include "optimization_config.h"

template<typename Config = OptimizationConfig::DefaultConfig>
class BitsetDirectory {
    private:
        // SIMD-optimized constants from config.h - DO NOT CHANGE without updating SIMD code
        static constexpr size_t L1_BITS = ::Config::BitsetConfig::L1_BITS;
        static constexpr size_t L2_BITS = ::Config::BitsetConfig::L2_BITS;
        static constexpr size_t CHUNK_SIZE = ::Config::BitsetConfig::CHUNK_SIZE;
        static constexpr size_t MAX_PRICE_LEVELS = ::Config::BitsetConfig::MAX_PRICE_LEVELS;
        
        uint64_t l1_bitset;           // Coarse-grain directory (which L2 chunks have data)
        uint64_t l2_bitset[L1_BITS];  // Fine-grain directory (which price levels have data)
        
    public:
        BitsetDirectory();
        
        void set_bit(uint32_t price_index);
        void clear_bit(uint32_t price_index);
        bool test_bit(uint32_t price_index) const;

        uint32_t find_highest_bit() const;
        uint32_t find_lowest_bit() const;
        uint32_t find_next_higher_bit(uint32_t from_index) const; 
        uint32_t find_next_lower_bit(uint32_t from_index) const;
        
        bool has_any_bits() const;
        void clear_all();
        
        // Use AVX2 to scan multiple L2 bitsets in parallel (conditionally compiled)
        template<bool UseSimd = Config::USE_SIMD>
        std::enable_if_t<UseSimd, uint32_t> simd_scan_l2_forward(uint32_t start_chunk) const;
        
        template<bool UseSimd = Config::USE_SIMD>
        std::enable_if_t<UseSimd, uint32_t> simd_scan_l2_backward(uint32_t start_chunk) const;
        
        template<bool UseSimd = Config::USE_SIMD>
        std::enable_if_t<!UseSimd, uint32_t> simd_scan_l2_forward(uint32_t start_chunk) const {
            return scalar_scan_l2_forward(start_chunk);
        }
        
        template<bool UseSimd = Config::USE_SIMD>
        std::enable_if_t<!UseSimd, uint32_t> simd_scan_l2_backward(uint32_t start_chunk) const {
            return scalar_scan_l2_backward(start_chunk);
        }
        
        bool validate_consistency() const;

    private: // HELPERS
        static constexpr uint32_t get_l1_index(uint32_t price_index);
        static constexpr uint32_t get_l2_bit(uint32_t price_index);
        
        // Scalar fallback implementations
        uint32_t scalar_scan_l2_forward(uint32_t start_index) const;
        uint32_t scalar_scan_l2_backward(uint32_t start_index) const;
};

// ============================================================================
// TYPE ALIASES FOR COMMON CONFIGURATIONS
// ============================================================================

// Main BitsetDirectory type - uses configuration selected by CMake
using BitsetDirectoryDefault = BitsetDirectory<OptimizationConfig::DefaultConfig>;

// Specific configuration aliases for benchmarking
using FullyOptimizedBitsetDirectory = BitsetDirectory<OptimizationConfig::FullyOptimizedConfig>;
using ScalarBaselineBitsetDirectory = BitsetDirectory<OptimizationConfig::ScalarBaselineConfig>;