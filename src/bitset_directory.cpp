#include "bitset_directory.h"

template<typename Config>
BitsetDirectory<Config>::BitsetDirectory(): l1_bitset(0) {
    for (size_t i = 0; i < L1_BITS; ++i) {
        l2_bitset[i] = 0;
    }
}

template<typename Config>
void BitsetDirectory<Config>::set_bit(uint32_t price_index) {
    uint32_t l1_index = get_l1_index(price_index);
    uint32_t l2_bit = get_l2_bit(price_index);
    l1_bitset |= (1ULL << l1_index);         // shift 1 to the l1_index-th bit position.
    l2_bitset[l1_index] |= (1ULL << l2_bit); // take the current l1_bitset value and set that bit to 1 without affecting other bits
}

template<typename Config>
void BitsetDirectory<Config>::clear_bit(uint32_t price_index) {
    uint32_t l1_index = get_l1_index(price_index);
    uint32_t l2_bit = get_l2_bit(price_index);
    l2_bitset[l1_index] &= ~(1ULL << l2_bit);
    // Only clear L1 bit if entire L2 chunk is now empty
    if (l2_bitset[l1_index] == 0) {
        l1_bitset &= ~(1ULL << l1_index);
    }
}

template<typename Config>
bool BitsetDirectory<Config>::test_bit(uint32_t price_index) const {
    uint32_t l1_index = get_l1_index(price_index);
    uint32_t l2_bit = get_l2_bit(price_index);
    return (l1_bitset & (1ULL << l1_index)) && (l2_bitset[l1_index] & (1ULL << l2_bit));
}

template<typename Config>
uint32_t BitsetDirectory<Config>::find_highest_bit() const {
    if (l1_bitset == 0) return MAX_PRICE_LEVELS;
    
    if constexpr (Config::USE_SIMD) {
        // Find highest set bit in L1 using intrinsics (x86: LZCNT, ARM: CLZ) - O(1) 
        int highest_l1 = 63 - __builtin_clzll(l1_bitset);
        
        for (int i=highest_l1; i>=0; i--) {
            uint64_t l2_chunk = l2_bitset[i];
            if (l2_chunk != 0) {
                int highest_l2 = 63 - __builtin_clzll(l2_chunk); // Find highest bit in this L2 chunk 
                return i * CHUNK_SIZE + highest_l2;
            }
        }
    } 
    else {
        // Scalar fallback - simple loop-based search
        for (int i = L1_BITS - 1; i >= 0; i--) {
            uint64_t l2_chunk = l2_bitset[i];
            if (l2_chunk != 0) {
                // Find highest bit in this L2 chunk using simple loop
                for (int j = L2_BITS - 1; j >= 0; j--) {
                    if (l2_chunk & (1ULL << j)) {
                        return i * CHUNK_SIZE + j;
                    }
                }
            }
        }
    }
    return MAX_PRICE_LEVELS;
}

template<typename Config>
uint32_t BitsetDirectory<Config>::find_lowest_bit() const {
    if (l1_bitset == 0) return MAX_PRICE_LEVELS;

    if constexpr (Config::USE_SIMD) {
        // Find lowest set bit in L1 using intrinsics (x86: TZCNT, ARM: CTZ) - O(1) 
        int lowest_l1 = __builtin_ctzll(l1_bitset);
        
        // Search from lowest L1 chunk upward
        for (size_t i=lowest_l1; i<L1_BITS; i++) {
            uint64_t l2_chunk = l2_bitset[i];
            if (l2_chunk != 0) {
                int lowest_l2 = __builtin_ctzll(l2_chunk);
                return i * CHUNK_SIZE + lowest_l2;
            }
        }
    }
    else {
        // Scalar fallback - simple loop-based search
        for (size_t i = 0; i < L1_BITS; i++) {
            uint64_t l2_chunk = l2_bitset[i];
            if (l2_chunk != 0) {
                // Find lowest bit in this L2 chunk using simple loop
                for (size_t j = 0; j < L2_BITS; j++) {
                    if (l2_chunk & (1ULL << j)) {
                        return i * CHUNK_SIZE + j;
                    }
                }
            }
        }
    }
    return MAX_PRICE_LEVELS;
}

template<typename Config>
uint32_t BitsetDirectory<Config>::find_next_higher_bit(uint32_t from_index) const { 
    if (l1_bitset == 0 || from_index >= MAX_PRICE_LEVELS - 1) return MAX_PRICE_LEVELS;

    // O(n):
    // for (uint32_t i=from_index + 1; i<MAX_PRICE_LEVELS; i++) {
    //     if (test_bit(i)) return i;
    // } 

    // O(log_n):
    uint32_t l1_index = get_l1_index(from_index);
    uint32_t l2_bit = get_l2_bit(from_index);

    // *Check current chunk for higher bits*
    uint64_t current_l2 = l2_bitset[l1_index];
    // BIT SHIFT OVERFLOW FIX: 1ULL << 64 is undefined behavior, was causing infinite loops
    // when bit position = 63, need to check for this edge case
    uint64_t mask;
    if (l2_bit >= 63) {
        mask = 0; // No higher bits possible in this chunk
    } else {
        mask = ~((1ULL << (l2_bit + 1)) - 1);
    }
    uint64_t masked_chunk = current_l2 & mask;
    if (masked_chunk != 0) {
        // Both SIMD and scalar use the same optimized intrinsic for single bit operations
        // The real SIMD optimization is in bulk scanning (simd_scan_l2_forward)
        int next_bit = __builtin_ctzll(masked_chunk);
        return l1_index * CHUNK_SIZE + next_bit;
    }

    // *Search higher L1 chunks*
    if constexpr (Config::USE_SIMD) {
        return simd_scan_l2_forward<true>(l1_index + 1);
    } else {
        for (uint32_t i = l1_index + 1; i < L1_BITS; i++) {
            if (l2_bitset[i] != 0) {
                int next_bit = __builtin_ctzll(l2_bitset[i]);
                return i * CHUNK_SIZE + next_bit;
            }
        }
    }

    return MAX_PRICE_LEVELS;
}

template<typename Config>
uint32_t BitsetDirectory<Config>::find_next_lower_bit(uint32_t from_index) const {
    if (l1_bitset == 0 || from_index == 0) return MAX_PRICE_LEVELS;

    uint32_t l1_index = get_l1_index(from_index);
    uint32_t l2_bit = get_l2_bit(from_index);

    // Check current chunk for lower bits
    uint64_t current_l2 = l2_bitset[l1_index];
    uint64_t mask = (1ULL << l2_bit) - 1; 
    uint64_t masked_chunk = current_l2 & mask;
    if (masked_chunk != 0) {
        // Find highest bit in masked range (next lower bit)
        // Use optimized intrinsic for single bit operations
        int next_bit = 63 - __builtin_clzll(masked_chunk);
        return l1_index * CHUNK_SIZE + next_bit;
    }

    // Search lower L1 chunks (start from l1_index - 1)
    if constexpr (Config::USE_SIMD) {
        return simd_scan_l2_backward<true>(l1_index - 1);
    } else {
        for (uint32_t i = l1_index - 1; i != UINT32_MAX; i--) {
            if (l2_bitset[i] != 0) {
                int next_bit = 63 - __builtin_clzll(l2_bitset[i]);
                return i * CHUNK_SIZE + next_bit;
            }
        }
    }

    return MAX_PRICE_LEVELS;
}

template<typename Config>
bool BitsetDirectory<Config>::has_any_bits() const { 
    return l1_bitset != 0; 
}

template<typename Config>
void BitsetDirectory<Config>::clear_all() {
    l1_bitset = 0;
    for (size_t i = 0; i < L1_BITS; ++i) {
        l2_bitset[i] = 0;
    } 
}

template<typename Config>
template<bool UseSimd>
std::enable_if_t<UseSimd, uint32_t> BitsetDirectory<Config>::simd_scan_l2_forward(uint32_t start_index) const {
    uint32_t start_chunk = get_l1_index(start_index);
    
    // Align to 4-chunk boundaries for optimal SIMD performance
    uint32_t aligned_start = (start_chunk + 3) & ~3; // Round up to nearest multiple of 4
    constexpr uint32_t vec_size = 4; // 256-bit / 64-bit = 4 lanes
    
    // Handle any unaligned chunks at the beginning with scalar code
    for (uint32_t i = start_chunk; i < aligned_start && i < L1_BITS; i++) {
        if (l2_bitset[i] != 0) {
            int first_bit = __builtin_ctzll(l2_bitset[i]);
            uint32_t found_index = i * CHUNK_SIZE + first_bit;
            if (found_index > start_index) {
                return found_index;
            }
        }
    }
    
    for (uint32_t i = aligned_start; i < L1_BITS; i += vec_size) {
        // Load 4 consecutive 64-bit chunks (unaligned)
        __m256i vec = _mm256_loadu_si256((const __m256i*)&l2_bitset[i]);
        
        // Check for non-zero chunks (inverted comparison)
        __m256i zero_vec = _mm256_setzero_si256();
        __m256i cmp_result = _mm256_cmpeq_epi64(vec, zero_vec);
        uint32_t mask = _mm256_movemask_epi8(cmp_result);
        
        // If mask != 0xFFFFFFFF, at least one chunk is non-zero
        if (mask != 0xFFFFFFFF) {
            // Convert byte mask to lane mask (each lane is 8 bytes)
            uint32_t lane_mask = ~mask; // Invert to get non-zero lanes
            
            // Process lanes from left to right (ascending order)
            for (uint32_t j = 0; j < vec_size; j++) {
                uint32_t lane_byte_offset = j * 8;
                if ((lane_mask >> lane_byte_offset) & 0xFF) { // Check if lane j is non-zero
                    uint32_t chunk_idx = i + j;
                    if (chunk_idx >= L1_BITS) break;
                    
                    uint64_t chunk_value = l2_bitset[chunk_idx];
                    if (chunk_value != 0) {
                        int first_bit = __builtin_ctzll(chunk_value);
                        uint32_t found_index = chunk_idx * CHUNK_SIZE + first_bit;
                        if (found_index > start_index) {
                            return found_index;
                        }
                    }
                }
            }
        }
    }

    return MAX_PRICE_LEVELS;
}

template<typename Config>
template<bool UseSimd>
std::enable_if_t<UseSimd, uint32_t> BitsetDirectory<Config>::simd_scan_l2_backward(uint32_t start_index) const {
    uint32_t start_chunk = get_l1_index(start_index);
    constexpr uint32_t vec_size = 4;
    
    uint32_t aligned_end = (start_chunk + 1) & ~3; 
    for (uint32_t i = start_chunk; i > aligned_end && i != UINT32_MAX; i--) {
        if (l2_bitset[i] != 0) {
            int last_bit = 63 - __builtin_clzll(l2_bitset[i]);
            uint32_t found_index = i * CHUNK_SIZE + last_bit;
            if (found_index < start_index) {
                return found_index;
            }
        }
    }
    
    for (uint32_t i = aligned_end; i != UINT32_MAX; i -= vec_size) {
        if (i < vec_size) break; // Prevent underflow
        
        uint32_t chunk_start = i - vec_size;
        __m256i vec = _mm256_loadu_si256((const __m256i*)&l2_bitset[chunk_start]);        
        __m256i zero_vec = _mm256_setzero_si256();
        __m256i cmp_result = _mm256_cmpeq_epi64(vec, zero_vec);
        uint32_t mask = _mm256_movemask_epi8(cmp_result);
        
        if (mask != 0xFFFFFFFF) {
            uint32_t lane_mask = ~mask; // Invert to get non-zero lanes
            
            for (uint32_t j = vec_size; j > 0; j--) {
                uint32_t lane_idx = j - 1;
                uint32_t lane_byte_offset = lane_idx * 8;
                
                if ((lane_mask >> lane_byte_offset) & 0xFF) {
                    uint32_t chunk_idx = chunk_start + lane_idx;
                    if (chunk_idx >= L1_BITS) continue;
                    
                    uint64_t chunk_value = l2_bitset[chunk_idx];
                    if (chunk_value != 0) {
                        int last_bit = 63 - __builtin_clzll(chunk_value);
                        uint32_t found_index = chunk_idx * CHUNK_SIZE + last_bit;
                        if (found_index < start_index) {
                            return found_index;
                        }
                    }
                }
            }
        }
    }
    
    return MAX_PRICE_LEVELS;
}

template<typename Config>
bool BitsetDirectory<Config>::validate_consistency() const {
    for (uint32_t i = 0; i < L1_BITS; i++) {
        bool l1_bit_set = (l1_bitset & (1ULL << i)) != 0;
        bool l2_has_data = (l2_bitset[i] != 0);
        // L1 bit set implies L2 has data
        if (l1_bit_set && !l2_has_data) {
            return false; // Orphaned L1 bit
        }
        // L2 has data implies L1 bit set
        if (l2_has_data && !l1_bit_set) {
            return false; // Missing L1 directory entry
        }
    }
    return true;
}

template<typename Config>
constexpr uint32_t BitsetDirectory<Config>::get_l1_index(uint32_t price_index) {
    return price_index / CHUNK_SIZE;
}

template<typename Config>
constexpr uint32_t BitsetDirectory<Config>::get_l2_bit(uint32_t price_index) {
    return price_index % CHUNK_SIZE;
}

// ============================================================================
// SCALAR FALLBACK IMPLEMENTATIONS
// ============================================================================

template<typename Config>
uint32_t BitsetDirectory<Config>::scalar_scan_l2_forward(uint32_t start_index) const {
    uint32_t start_chunk = get_l1_index(start_index);
    
    for (uint32_t i = start_chunk; i < L1_BITS; i++) {
        if (l2_bitset[i] != 0) {
            for (uint32_t j = 0; j < L2_BITS; j++) {
                if (l2_bitset[i] & (1ULL << j)) {
                    uint32_t found_index = i * CHUNK_SIZE + j;
                    if (found_index > start_index) {
                        return found_index;
                    }
                }
            }
        }
    }
    return MAX_PRICE_LEVELS;
}

template<typename Config>
uint32_t BitsetDirectory<Config>::scalar_scan_l2_backward(uint32_t start_index) const {
    uint32_t start_chunk = get_l1_index(start_index);
    
    for (uint32_t i = start_chunk; i != UINT32_MAX; i--) {
        if (l2_bitset[i] != 0) {
            for (uint32_t j = L2_BITS - 1; j != UINT32_MAX; j--) {
                if (l2_bitset[i] & (1ULL << j)) {
                    uint32_t found_index = i * CHUNK_SIZE + j;
                    if (found_index < start_index) {
                        return found_index;
                    }
                }
            }
        }
    }
    return MAX_PRICE_LEVELS;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

// Instantiate templates for all supported configurations
template class BitsetDirectory<OptimizationConfig::FullyOptimizedConfig>;
template class BitsetDirectory<OptimizationConfig::ScalarBaselineConfig>;  
template class BitsetDirectory<OptimizationConfig::SimdOnlyConfig>;
template class BitsetDirectory<OptimizationConfig::MemoryOptimizedConfig>;
template class BitsetDirectory<OptimizationConfig::CacheOptimizedConfig>;
template class BitsetDirectory<OptimizationConfig::ObjectPoolOnlyConfig>;
template class BitsetDirectory<OptimizationConfig::ObjectPoolSimdConfig>;
