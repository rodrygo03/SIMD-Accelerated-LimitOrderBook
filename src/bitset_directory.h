#pragma once

#include <cstdint>
#include <immintrin.h>  // AVX2/AVX-512 intrinsics

#include "config.h"

class BitsetDirectory {
    private:
        // SIMD-optimized constants from config.h - DO NOT CHANGE without updating SIMD code
        static constexpr size_t L1_BITS = Config::BitsetConfig::L1_BITS;
        static constexpr size_t L2_BITS = Config::BitsetConfig::L2_BITS;
        static constexpr size_t CHUNK_SIZE = Config::BitsetConfig::CHUNK_SIZE;
        static constexpr size_t MAX_PRICE_LEVELS = Config::BitsetConfig::MAX_PRICE_LEVELS;
        
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
        
        // Use AVX2 to scan multiple L2 bitsets in parallel
        uint32_t simd_scan_l2_forward(uint32_t start_chunk) const;
        uint32_t simd_scan_l2_backward(uint32_t start_chunk) const;
        
        bool validate_consistency() const;

    private: // HELPERs
        static constexpr uint32_t get_l1_index(uint32_t price_index);
        static constexpr uint32_t get_l2_bit(uint32_t price_index);
};