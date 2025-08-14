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
        if constexpr (Config::USE_SIMD) {
            int next_bit = __builtin_ctzll(masked_chunk);
            return l1_index * CHUNK_SIZE + next_bit;
        } else {
            // Scalar fallback
            for (uint32_t j = l2_bit + 1; j < L2_BITS; j++) {
                if (masked_chunk & (1ULL << j)) {
                    return l1_index * CHUNK_SIZE + j;
                }
            }
        }
    }

    // *Search higher L1 chunks*
    for (uint32_t i=l1_index + 1; i<L1_BITS; i++)   {
        if (l2_bitset[i] != 0) {
            if constexpr (Config::USE_SIMD) {
                int next_bit = __builtin_ctzll(l2_bitset[i]);
                return i * CHUNK_SIZE + next_bit;
            } else {
                // Scalar fallback
                for (uint32_t j = 0; j < L2_BITS; j++) {
                    if (l2_bitset[i] & (1ULL << j)) {
                        return i * CHUNK_SIZE + j;
                    }
                }
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
        if constexpr (Config::USE_SIMD) {
            int next_bit = 63 - __builtin_clzll(masked_chunk);
            return l1_index * CHUNK_SIZE + next_bit;
        } else {
            // Scalar fallback
            for (uint32_t j = l2_bit - 1; j != UINT32_MAX; j--) {
                if (masked_chunk & (1ULL << j)) {
                    return l1_index * CHUNK_SIZE + j;
                }
            }
        }
    }

    // Search lower L1 chunks (start from l1_index - 1)
    for (uint32_t i=l1_index - 1; i!=UINT32_MAX; i--) {
        if (l2_bitset[i] != 0) {
            // Find highest bit in this chunk (next lower bit)
            if constexpr (Config::USE_SIMD) {
                int next_bit = 63 - __builtin_clzll(l2_bitset[i]);
                return i * CHUNK_SIZE + next_bit;
            } else {
                // Scalar fallback
                for (uint32_t j = L2_BITS - 1; j != UINT32_MAX; j--) {
                    if (l2_bitset[i] & (1ULL << j)) {
                        return i * CHUNK_SIZE + j;
                    }
                }
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
    // parameter is bit index, need to convert to chunk index
    uint32_t start_chunk = get_l1_index(start_index);
    uint32_t vec_size = 4; // 256 bit load / 64 bit lane
    for (uint32_t i=start_chunk; i<L1_BITS; i+=vec_size) {
        __m256i vec = _mm256_loadu_si256((const __m256i*)&l2_bitset[i]); // Unaligned load 4 consecutive L2 bitsets into AVX2 register (x86-64 CISC ISA)
        __m256i cmp_result = _mm256_cmpeq_epi64(vec, _mm256_setzero_si256()); // Check for non-zero lanes: 0xFFFF...F if lane equals zero
        int32_t mask = _mm256_movemask_epi8(cmp_result); // MSB of each byte -> 32-bit mask

        if (mask != (int32_t)0xFFFFFFFF) { // At least one lane is non-zero (has data)
            for (uint32_t j=0; j<vec_size; j++) { // FWD
                if (i+j < L1_BITS && l2_bitset[i+j] != 0) {
                    int first_bit = __builtin_ctzll(l2_bitset[i+j]); // Lowest bit
                    uint32_t found_index = (i + j) * CHUNK_SIZE + first_bit;
                    // Only return if found bit is after start_index
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
template<bool UseSimd>
std::enable_if_t<UseSimd, uint32_t> BitsetDirectory<Config>::simd_scan_l2_backward(uint32_t start_index) const {
    uint32_t start_chunk = get_l1_index(start_index);
    uint32_t vec_size = 4;
    for (uint32_t i=(start_chunk/vec_size)*vec_size; i!=UINT32_MAX; i-=vec_size) { // rounds down to the nearest multiple of 4
        __m256i vec = _mm256_loadu_si256((const __m256i*)&l2_bitset[i]); 
        __m256i cmp_result = _mm256_cmpeq_epi64(vec, _mm256_setzero_si256());
        int32_t mask = _mm256_movemask_epi8(cmp_result);

        if (mask != (int32_t)0xFFFFFFFF) { 
            for (uint32_t j=vec_size-1; j!=UINT32_MAX; j--) {
                uint32_t chunk_idx = i + j;
                if (chunk_idx <= start_chunk && chunk_idx < L1_BITS && l2_bitset[chunk_idx] != 0) {
                    int last_bit = 63 - __builtin_clzll(l2_bitset[chunk_idx]); // Highest bit
                    uint32_t found_index = chunk_idx * CHUNK_SIZE + last_bit;
                    // only return bits before start_index
                    if (found_index < start_index) {
                        return found_index;
                    }
                }
            }
        }
        
        if (i < vec_size) break; // Prevent underflow
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