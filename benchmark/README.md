# Benchmark Suite

This directory contains the comprehensive benchmarking framework for evaluating the engine's performance across different optimization configurations.

## Overview

The benchmark suite tests seven distinct optimization configurations against real market data, measuring both latency and throughput performance with detailed hardware metrics collection.

## Core Components

### Benchmark Framework
- **`benchmark_framework.cpp/h`**: Core framework providing standardized testing infrastructure
- **`benchmark_framework_impl.cpp`**: Implementation with separate latency and throughput test modes
- **`run_comprehensive_benchmark.cpp`**: Multi-configuration benchmark runner

### Market Data Processing
- **`nasdaq_itch_parser.cpp/h`**: NASDAQ ITCH 5.0 binary protocol parser
- **`data/`**: Real NASDAQ market data files from 2019
- **`find_max_events.sh`**: Utility to determine maximum processable events per data file

### Performance Measurement
- **`perf_counters.cpp`**: Linux perf integration for hardware performance counters
- **`cache_flusher.cpp`**: Cache invalidation utilities for consistent measurement

### Analysis & Visualization
- **`visualize_results.py`**: Comprehensive visualization pipeline generating:
  - Performance summary tables
  - Latency/throughput scatter plots
  - Hardware metrics analysis
  - Statistical comparisons across configurations
- **`results/`**: Output directory for benchmark data and visualizations

## Test Configurations

1. **scalar-baseline**: Pure STL implementation (no optimizations)
2. **simd-baseline**: SIMD-only optimization
3. **object-pool-only**: Memory optimization only
4. **object-pool-simd**: Combined memory + SIMD
5. **cache-only**: Cache optimization only
6. **memory-optimized**: All memory optimizations
7. **fully-optimized**: All optimizations enabled

## Running Benchmarks

Execute comprehensive testing across all configurations and data files:
```bash
cd benchmark/
./results.sh --all-files
```

Run specific tests:
```bash
./results.sh 16000 01302019    # Single file with 16K events
./results.sh --all-files       # All data files with default events
```

Visualize existing results:
```bash
python visualize_results.py results/
```

Find maximum processable events for a data file:
```bash
./find_max_events.sh scalar-baseline  # Test memory limits with configuration
```

## Key Metrics

- **Latency**: Per-operation timing (mean, P99, P99.9)
- **Throughput**: Sustained operations per second
- **Hardware**: CPU cycles, cache miss rates, memory bandwidth
- **Memory**: Peak memory usage and allocation patterns

## Test Methodology

Each benchmark processes real NASDAQ ITCH data through two distinct test modes:

1. **Latency Test**: Measures individual operation timing with data cycling
2. **Throughput Test**: Measures sustained processing rate over time

Results are automatically separated by test type to ensure accurate performance comparison across optimization strategies.