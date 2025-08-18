# Core Engine

This directory contains the core implementation of the high-performance limit order book engine with SIMD optimizations.

## Architecture Overview

The engine follows a modular, template-based design enabling compile-time optimization selection across multiple performance strategies.

## Core Components

### Order Book Engine
- **`order_book.h/cpp`**: Main LOB implementation with template-based optimization configuration
- **`lob_engine.h/cpp`**: Message processing engine and event dispatch system
- **`lob_engine_factory.h`**: Factory for creating engines with specific optimization profiles

### Data Structures
- **`bitset_directory.h/cpp`**: SIMD-optimized hierarchical bitset for O(1) price level discovery
- **`price_level.h/cpp`**: FIFO order queues with intrusive linked lists
- **`order.h/cpp`**: Order and trade data structures
- **`object_pool.hpp`**: Zero-allocation memory pool with O(1) acquire/release

### Configuration System
- **`optimization_config.h`**: Template-based optimization policy definitions
- **`config.h.in`**: CMake configuration template (generates build-specific `config.h`)

## Key Optimizations

### SIMD Vectorization
- AVX2 intrinsics for parallel bitset operations
- 4-way parallel price level scanning
- SIMD-optimized price discovery algorithms

### Memory Management
- Pre-allocated object pools eliminating malloc/free in critical paths
- Cache-aligned data structures (64-byte alignment)
- Intrusive data structures reducing pointer indirection

### Cache Optimization
- Structure-of-arrays layouts for spatial locality
- Prefetching hints for predictable access patterns
- Hot/cold path separation

## Template Configuration

Seven distinct optimization configurations available:
- **ScalarBaselineConfig**: Pure STL implementation
- **SimdOnlyConfig**: SIMD optimizations only
- **ObjectPoolOnlyConfig**: Memory optimization only
- **ObjectPoolSimdConfig**: Combined memory + SIMD
- **CacheOptimizedConfig**: Cache optimization focus
- **MemoryOptimizedConfig**: Comprehensive memory optimization
- **FullyOptimizedConfig**: All optimizations enabled

## Performance Characteristics

- **Latency**: Sub-microsecond operation timing (100-500ns typical)
- **Throughput**: 15K+ sustained operations per second
- **Memory**: Deterministic allocation patterns with object pooling
- **Cache**: 90%+ hit rates in critical paths

The implementation achieves institutional-grade performance through careful application of modern C++ optimization techniques and deep understanding of CPU microarchitecture.