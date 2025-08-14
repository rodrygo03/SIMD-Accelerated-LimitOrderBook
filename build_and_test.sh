#!/bin/bash


echo "Building SIMD ACCELERATED LOB system..."

mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)

echo -e "\nBuild complete. Running tests...\n"

# Run individual test suites
echo "Running BitsetDirectory tests..."
./test_bitset_directory

echo -e "\nRunning OrderBook tests..."
./test_order_book

echo -e "\nRunning LOBEngine tests..."
./test_lob_engine

echo -e "\nRunning CTest..."
ctest --output-on-failure

echo -e "\nBuild and test complete!"