#!/bin/bash
# Simple build and test script - delegates to enhanced script

echo "Delegating to enhanced build script with CMake presets..."
echo ""

# Check if enhanced script exists
ENHANCED_SCRIPT="./scripts/build_and_test.sh"
if [ -f "$ENHANCED_SCRIPT" ]; then
    exec "$ENHANCED_SCRIPT" "$@"
else
    echo "Enhanced script not found. Using basic build..."
    echo ""
    
    echo "Building SIMD ACCELERATED LOB system..."
    
    mkdir -p build
    cd build
    
    # Use CMake preset if available, otherwise fall back to basic config
    if [ -f "../CMakePresets.json" ]; then
        echo "Using default CMake preset..."
        cd ..
        cmake --preset=default || {
            echo "Preset failed, falling back to basic configuration"
            cd build
            cmake .. -DCMAKE_BUILD_TYPE=Release
        }
        cd build
    else
        cmake .. -DCMAKE_BUILD_TYPE=Release
    fi
    
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
    echo ""
    echo "TIP: Use ./scripts/build_and_test.sh for enhanced features and CMake presets"
fi