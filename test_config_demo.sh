#!/bin/bash
# Demo script showing configuration flexibility

echo "=== SIMD-LOB Configuration Demo ==="
echo ""

# Load current environment
source ./scripts/load_env.sh

echo "Current configuration from .env:"
echo "  DEFAULT_POOL_SIZE: $DEFAULT_POOL_SIZE"
echo "  BASE_PRICE: $BASE_PRICE ($(echo "scale=2; $BASE_PRICE/100" | bc) dollars)"
echo "  MAX_PRICE_LEVELS: $MAX_PRICE_LEVELS"
echo ""

# Create a temporary modified .env for demo
echo "Creating demo configuration for penny stock trading..."
cat > .env.demo << EOF
# Demo configuration for penny stock trading
PROJECT_ROOT=$PROJECT_ROOT
BUILD_DIR=\${PROJECT_ROOT}/build_demo
CMAKE_BUILD_TYPE=Release
ENABLE_SIMD=ON
BUILD_SCALAR_BASELINE=ON

# Penny stock configuration
DEFAULT_POOL_SIZE=500000
BASE_PRICE=100
MIN_PRICE_TICK=1
MAX_PRICE_LEVELS=2048
TRADE_POOL_RATIO=5

# Enhanced benchmark settings
BENCHMARK_ITERATIONS=20000
WARMUP_ITERATIONS=2000

# Output directories
RESULTS_DIR=\${PROJECT_ROOT}/results_demo
LOGS_DIR=\${PROJECT_ROOT}/logs_demo
DATA_DIR=\${PROJECT_ROOT}/bench/data
EOF

echo "Demo .env created with:"
echo "  DEFAULT_POOL_SIZE: 500000 (smaller pool)"
echo "  BASE_PRICE: 100 (\$1.00 - penny stock)"
echo "  MAX_PRICE_LEVELS: 2048 (half the levels)"
echo "  BENCHMARK_ITERATIONS: 20000 (more iterations)"
echo ""

# Show how to build with custom configuration
echo "To build with custom configuration:"
echo "1. Edit .env file or create .env.custom"
echo "2. Run: DEFAULT_POOL_SIZE=2000000 BASE_PRICE=10000 ./scripts/build_with_env.sh"
echo "3. Or use compiler defines directly:"
echo "   cmake -B build_custom -DCMAKE_CXX_FLAGS=\"-DDEFAULT_POOL_SIZE_VALUE=2000000 -DBASE_PRICE_VALUE=10000\""
echo ""

# Show environment switching
echo "Example: Switching to demo environment:"
echo "export PROJECT_ROOT=$PROJECT_ROOT"
echo "export BUILD_DIR=$PROJECT_ROOT/build_demo"
echo "export DEFAULT_POOL_SIZE=500000"
echo "export BASE_PRICE=100"
echo "export MAX_PRICE_LEVELS=2048"
echo ""

# Show configuration presets
echo "Common configuration presets available in config.h:"
echo "  CONFIG_PENNY_STOCK()  - \$1.00 base price"
echo "  CONFIG_NORMAL_STOCK() - \$50.00 base price"  
echo "  CONFIG_HIGH_PRICE()   - \$500.00 base price"
echo ""
echo "  CONFIG_SMALL_POOL()   - 10,000 orders"
echo "  CONFIG_MEDIUM_POOL()  - 100,000 orders"
echo "  CONFIG_LARGE_POOL()   - 1,000,000 orders"
echo ""

# Cleanup
echo "Cleaning up demo files..."
rm -f .env.demo

echo "Configuration demo complete!"
echo ""
echo "Key benefits:"
echo "  ✓ No hardcoded constants - all configurable"
echo "  ✓ Environment-specific settings via .env files"
echo "  ✓ Compile-time optimization with runtime flexibility"
echo "  ✓ Easy switching between market types (stocks, forex, crypto)"
echo "  ✓ Configurable pool sizes for different memory constraints"