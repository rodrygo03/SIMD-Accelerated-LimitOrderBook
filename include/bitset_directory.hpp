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
    BitsetDirectory(): l1_bitset(0) {
        for (int i = 0; i < L1_BITS; ++i) {
            l2_bitset[i] = 0;
        }
    }
    
    void set_bit(uint32_t price_index) {
        uint32_t l1_index = get_l1_index(price_index);
        uint32_t l2_bit = get_l2_bit(price_index);
        l1_bitset |= (1ULL << l1_index);         // shift 1 to the l1_index-th bit position.
        l2_bitset[l1_index] |= (1ULL << l2_bit); // take the current l1_bitset value and set that bit to 1 without affecting other bits
    }
    
    // TODO: Mark price level as empty
    void clear_bit(uint32_t price_index);
    
    // TODO: Check if price level has orders
    bool test_bit(uint32_t price_index) const;
    
    // TODO: Find highest set bit (best ask for sells) using SIMD
    // Returns MAX_PRICE_LEVELS if no bits set
    uint32_t find_highest_bit() const;
    
    // TODO: Find lowest set bit (best bid for buys) using SIMD  
    // Returns MAX_PRICE_LEVELS if no bits set
    uint32_t find_lowest_bit() const;
    
    // TODO: Find next higher bit from given index (for market data walking)
    uint32_t find_next_higher_bit(uint32_t from_index) const;
    
    // TODO: Find next lower bit from given index
    uint32_t find_next_lower_bit(uint32_t from_index) const;
    
    // TODO: Check if any bits are set
    bool has_any_bits() const { return l1_bitset != 0; }
    
    // TODO: Clear all bits (used for reset)
    void clear_all();
    
    // TODO: SIMD-accelerated batch scan of L2 bitsets
    // Use AVX2 to scan multiple L2 bitsets in parallel
    uint32_t simd_scan_l2_forward(uint32_t start_chunk) const;
    uint32_t simd_scan_l2_backward(uint32_t start_chunk) const;
    
    // TODO: Validate bitset consistency (debugging)
    bool validate_consistency() const;

  private: // HELPERs
    static constexpr uint32_t get_l1_index(uint32_t price_index) {
        return price_index / CHUNK_SIZE;
    }
    
    static constexpr uint32_t get_l2_bit(uint32_t price_index) {
        return price_index % CHUNK_SIZE;
    }
};