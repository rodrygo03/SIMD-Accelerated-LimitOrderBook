# High-Performance Limit Order Book Engine

A production-grade limit order book implementation demonstrating advanced computer architecture optimization through SIMD vectorization, cache-aware programming, and template metaprogramming.

## Background

Modern financial markets process millions of orders per second, requiring ultra-low latency order matching systems. Traditional implementations face performance bottlenecks due to sequential processing and inefficient memory access patterns. This project addresses these challenges through systematic application of computer architecture optimization techniques.

Limit order books maintain price-time priority queues for buy and sell orders. The core algorithmic challenge lies in efficiently finding the best bid/ask prices among thousands of price levels while maintaining FIFO ordering within each level. This implementation achieves O(1) price discovery through a novel hierarchical bitset directory optimized with AVX2 SIMD instructions.

## Implementation Overview

### Architecture Design

The engine follows a template-based architecture enabling compile-time optimization selection. Seven distinct configurations test different optimization strategies:

- **Scalar Baseline**: Pure STL implementation for baseline comparison
- **SIMD Only**: AVX2 vectorization for parallel bit operations  
- **Object Pool Only**: Zero-allocation memory management
- **Object Pool + SIMD**: Combined memory and vectorization optimization
- **Cache Optimized**: Cache-aware data layout and prefetching
- **Memory Optimized**: Comprehensive memory optimization strategies
- **Fully Optimized**: All optimization techniques combined

### Key Innovations

**SIMD-Optimized Price Discovery**: Custom AVX2 implementation processes four 64-bit price levels simultaneously, achieving 4x speedup over scalar bit scanning operations.

**Hierarchical Bitset Directory**: Two-level bitset structure provides O(1) best price discovery across 4096 price levels with minimal memory overhead.

**Zero-Allocation Object Pooling**: Pre-allocated memory pools eliminate malloc/free calls in critical paths, ensuring deterministic performance.

**Cache-Aware Data Layout**: 64-byte aligned structures and structure-of-arrays patterns optimize cache utilization for 90%+ hit rates.

## Performance Results

### Test Hardware Configuration
- **CPU**: Intel Core i7-1165G7 (11th Gen, Tiger Lake)
- **Architecture**: x86_64 with AVX2 SIMD support
- **Clock Speed**: 2.8GHz base, 4.7GHz boost (single-threaded performance)
- **Cache**: L1d 192KB, L1i 128KB, L2 5MB, L3 12MB
- **Memory**: 16GB DDR4 system RAM

![Performance Summary](benchmark/visualizations/summary_table.png)

### Hardware Performance Metrics
![Hardware Metrics](benchmark/visualizations/hardware_summary_table.png)

Detailed hardware analysis reveals:
- CPU cycles per operation: 300-500 cycles (optimized configurations)
- Cache miss rates: <5% L1, <2% L2, <1% L3 for critical paths
- Instructions per cycle: 1.2-1.8 IPC depending on configuration
- Memory bandwidth utilization: 2-4 GB/s sustained

### Performance Trade-offs
![Latency vs Throughput](benchmark/visualizations/throughput_vs_latency.png)

The analysis reveals that SIMD and memory optimizations provide the most significant performance improvements, while reaching a practical performance ceiling dominated by I/O parsing rather than computational bottlenecks.

## Technical Highlights

### SIMD Vectorization
```cpp
// AVX2 parallel bitset scanning
__m256i vec = _mm256_loadu_si256((const __m256i*)&l2_bitset[i]);
__m256i cmp_result = _mm256_cmpeq_epi64(vec, zero_vec);
uint32_t mask = _mm256_movemask_epi8(cmp_result);
```

### Template Metaprogramming
```cpp
template<typename Config>
class OrderBook {
    if constexpr (Config::USE_SIMD) {
        return simd_scan_l2_forward<true>(start_chunk);
    } else {
        return scalar_scan_l2_forward(start_chunk);
    }
};
```

### Hardware Performance Integration
The benchmark suite integrates Linux perf counters for comprehensive hardware performance analysis, measuring CPU cycles, cache behavior, and memory bandwidth utilization across different optimization strategies.

## Repository Structure

```
├── src/                    # Core engine implementation
│   ├── order_book.{h,cpp}  # Main LOB with template optimizations
│   ├── bitset_directory.*  # SIMD-optimized price discovery
│   ├── object_pool.hpp     # Zero-allocation memory management
│   └── optimization_config.h # Template configuration policies
├── benchmark/              # Comprehensive benchmarking suite  
│   ├── benchmark_framework.* # Standardized testing infrastructure
│   ├── nasdaq_itch_parser.* # Real market data processing
│   ├── visualize_results.py # Performance analysis and visualization
│   └── data/               # NASDAQ ITCH market data files
├── tests/                  # Unit test suite
├── scripts/                # Development automation scripts
└── build-*/                # Configuration-specific build directories
```

## Building and Testing

The project uses CMake with preset configurations for each optimization strategy:

```bash
# Build all configurations
cmake --preset fully-optimized
cmake --build build-fully-optimized

# Run comprehensive benchmarks
cd benchmark && ./results.sh --all-files

# Generate visualizations (included in benchmark run)
python visualize_results.py results/
```

## Results and Analysis

Testing with real NASDAQ ITCH market data demonstrates that the implementation achieves performance characteristics competitive with institutional trading systems. The SIMD and memory optimizations provide significant improvements, with the fully optimized configuration achieving:

- **120ns mean latency** per operation
- **15K+ sustained throughput** operations per second  
- **Sub-microsecond P99.9 latency** for tail performance
- **Deterministic performance** through zero-allocation design

The performance ceiling analysis reveals that further improvements require addressing I/O parsing bottlenecks rather than computational optimization, indicating successful achievement of CPU-bound performance targets.

This implementation demonstrates practical application of advanced computer architecture concepts in a financially relevant domain, showcasing the intersection of systems programming, performance optimization, and quantitative finance.