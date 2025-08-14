#pragma once

#include <cstdint>
#include <immintrin.h>  // AVX2/AVX-512 intrinsics

class BitsetDirectory {
    private:
        static constexpr size_t L1_BITS = 64;     // 64-bit L1 bitset
        static constexpr size_t L2_BITS = 64;     // 64-bit L2 bitsets
        static constexpr size_t CHUNK_SIZE = 64;  // Price levels per L2 chunk
        static constexpr size_t MAX_PRICE_LEVELS = L1_BITS * L2_BITS; // 4096 levels
        
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