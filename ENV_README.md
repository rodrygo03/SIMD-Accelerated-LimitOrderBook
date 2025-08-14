# Environment Configuration Guide

This project uses environment variables to manage paths and configuration, making it portable across different development environments.

## Quick Start

### 1. Initial Setup
```bash
# Run the setup script to configure your environment
./scripts/setup_dev_env.sh
```

### 2. Load Environment
```bash
# Load environment variables in your current shell
source ./scripts/load_env.sh
```

### 3. Build and Test
```bash
# Use the enhanced build script
./scripts/build_and_test.sh
```

## Environment Variables

### Core Paths
| Variable | Description | Default |
|----------|-------------|---------|
| `PROJECT_ROOT` | Project root directory | `pwd` |
| `BUILD_DIR` | CMake build directory | `${PROJECT_ROOT}/build` |
| `SRC_DIR` | Source code directory | `${PROJECT_ROOT}/src` |
| `TEST_DIR` | Test directory | `${PROJECT_ROOT}/tests` |
| `BENCH_DIR` | Benchmark directory | `${PROJECT_ROOT}/bench` |
| `SCALAR_DIR` | Scalar implementation directory | `${BENCH_DIR}/scalar` |

### Test Executables
| Variable | Description | Path |
|----------|-------------|------|
| `BITSET_TEST_EXEC` | BitsetDirectory test | `${BUILD_DIR}/test_bitset_directory` |
| `ORDER_BOOK_TEST_EXEC` | OrderBook test | `${BUILD_DIR}/test_order_book` |
| `LOB_ENGINE_TEST_EXEC` | LOBEngine test | `${BUILD_DIR}/test_lob_engine` |
| `SCALAR_TEST_EXEC` | Scalar implementation test | `${BUILD_DIR}/bench/scalar/test_scalar` |
| `ALL_TESTS_EXEC` | Combined test runner | `${BUILD_DIR}/run_all_tests` |

### Build Configuration
| Variable | Description | Default |
|----------|-------------|---------|
| `CMAKE_BUILD_TYPE` | CMake build type | `Release` |
| `ENABLE_SIMD` | Enable SIMD optimizations | `ON` |
| `BUILD_SCALAR_BASELINE` | Build scalar baseline | `ON` |
| `CXX_FLAGS` | Optimized compiler flags | `-O3 -march=native -mavx2` |
| `SCALAR_CXX_FLAGS` | Scalar compiler flags | `-O2` |

### Performance Testing
| Variable | Description | Default |
|----------|-------------|---------|
| `DEFAULT_POOL_SIZE` | Default object pool size | `1000000` |
| `TRADE_POOL_RATIO` | Trade pool size ratio (orders/trades) | `10` |
| `BENCHMARK_ITERATIONS` | Benchmark iterations | `10000` |
| `WARMUP_ITERATIONS` | Warmup iterations | `1000` |

### Order Book Configuration
| Variable | Description | Default |
|----------|-------------|---------|
| `MAX_PRICE_LEVELS` | Maximum price levels (up to 4096) | `4096` |
| `BASE_PRICE` | Base price in cents (e.g., 50000 = $500.00) | `50000` |
| `MIN_PRICE_TICK` | Minimum price increment | `1` |
| `MAX_MARKET_DEPTH_LEVELS` | Market depth query levels | `10` |

### Debug Configuration
| Variable | Description | Default |
|----------|-------------|---------|
| `SIMD_LOB_DEBUG` | Enable debug mode | `unset` |
| `VERBOSE_LOGGING` | Enable verbose logging | `unset` |

### Output Directories
| Variable | Description | Default |
|----------|-------------|---------|
| `RESULTS_DIR` | Benchmark results | `${PROJECT_ROOT}/results` |
| `LOGS_DIR` | Log files | `${PROJECT_ROOT}/logs` |
| `DATA_DIR` | Test data directory | `${BENCH_DIR}/data` |

## Manual Configuration

### 1. Edit .env File
```bash
# Copy and customize the environment file
cp .env .env.local
nano .env.local
```

### 2. Use Custom Environment
```bash
# Load custom environment
source ./scripts/load_env.sh .env.local
```

## Scripts Overview

### Setup Scripts
- `scripts/setup_dev_env.sh` - Initial environment setup with user input
- `scripts/load_env.sh` - Load environment variables from .env file

### Build Scripts  
- `scripts/build_and_test.sh` - Enhanced build and test with environment support
- `build_and_test.sh` - Original build script (for compatibility)

## Troubleshooting

### Environment Not Loading
```bash
# Check if .env file exists
ls -la .env

# Manually load environment
source ./scripts/load_env.sh

# Check loaded variables
echo $PROJECT_ROOT
echo $BUILD_DIR
```

### Build Issues
```bash
# Clean build
rm -rf $BUILD_DIR
./scripts/build_and_test.sh

# Check CMake configuration
cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE
```

### Test Failures
```bash
# Run individual tests
$BITSET_TEST_EXEC
$ORDER_BOOK_TEST_EXEC
$LOB_ENGINE_TEST_EXEC
$SCALAR_TEST_EXEC

# Check test executable paths
ls -la $BUILD_DIR/
ls -la $BUILD_DIR/bench/scalar/
```
