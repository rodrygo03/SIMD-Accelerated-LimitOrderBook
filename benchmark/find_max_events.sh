#!/bin/bash
# Systematic approach to find maximum safe event count
# Usage: ./find_max_events.sh <config>

set -e

CONFIG=${1:-"fully-optimized"}
RESULTS_FILE="max_events_test_results.txt"
MAX_MEMORY_USAGE_MB=8000  # Conservative limit (8GB out of ~10GB available)

echo "Finding maximum event count for configuration: $CONFIG" | tee $RESULTS_FILE
echo "System Memory Available: $(free -h | grep Mem | awk '{print $7}')" | tee -a $RESULTS_FILE
echo "Data file size: $(ls -lh /home/rodrigoorozco/Desktop/SIMD-LOB/benchmark/data/01302019.NASDAQ_ITCH50 | awk '{print $5}')" | tee -a $RESULTS_FILE
echo "Starting systematic testing..." | tee -a $RESULTS_FILE
echo "" | tee -a $RESULTS_FILE

# Function to get current memory usage in MB
get_memory_usage() {
    ps -o pid,vsz,rss,comm -p $1 2>/dev/null | tail -1 | awk '{print $3/1024}'
}

# Function to monitor system memory
check_system_memory() {
    local available_mb=$(free -m | grep Mem | awk '{print $7}')
    echo $available_mb
}

# Test increasing event counts - Multi-file contains ~96K total order events
test_events=(1000 2000 4000 8000 16000 32000 64000 96000)

successful_max=0
failed_at=0

for events in "${test_events[@]}"; do
    echo "Testing $events events..." | tee -a $RESULTS_FILE
    
    # Check if we have enough memory before starting
    available_mem=$(check_system_memory)
    if [ $available_mem -lt 2000 ]; then
        echo "  SKIPPED: Insufficient system memory ($available_mem MB)" | tee -a $RESULTS_FILE
        break
    fi
    
    # Start benchmark in background and monitor
    timeout 300s ./run_single_benchmark.sh $CONFIG $events > temp_benchmark_output.log 2>&1 &
    benchmark_pid=$!
    
    # Monitor memory usage
    max_memory_seen=0
    memory_exceeded=false
    
    # Monitor for up to 5 minutes
    for i in {1..300}; do
        if ! kill -0 $benchmark_pid 2>/dev/null; then
            # Process finished
            break
        fi
        
        current_memory=$(get_memory_usage $benchmark_pid)
        if (( $(echo "$current_memory > $max_memory_seen" | bc -l) )); then
            max_memory_seen=$current_memory
        fi
        
        # Check if memory usage exceeds limit
        if (( $(echo "$current_memory > $MAX_MEMORY_USAGE_MB" | bc -l) )); then
            echo "  TERMINATING: Memory usage exceeded limit ($current_memory MB > $MAX_MEMORY_USAGE_MB MB)" | tee -a $RESULTS_FILE
            kill $benchmark_pid 2>/dev/null || true
            memory_exceeded=true
            break
        fi
        
        sleep 1
    done
    
    # Check if process is still running (timeout)
    if kill -0 $benchmark_pid 2>/dev/null; then
        echo "  TIMEOUT: Benchmark took longer than 5 minutes" | tee -a $RESULTS_FILE
        kill $benchmark_pid 2>/dev/null || true
        failed_at=$events
        break
    fi
    
    # Check exit status
    wait $benchmark_pid
    exit_status=$?
    
    if [ $exit_status -eq 0 ] && [ "$memory_exceeded" = false ]; then
        successful_max=$events
        echo "  SUCCESS: Completed with max memory usage: ${max_memory_seen}MB" | tee -a $RESULTS_FILE
        
        # Extract performance metrics
        if [ -f temp_benchmark_output.log ]; then
            throughput=$(grep "Throughput:" temp_benchmark_output.log | tail -1 | awk '{print $2}')
            mean_latency=$(grep "Mean:" temp_benchmark_output.log | tail -1 | awk '{print $2}')
            echo "    Throughput: $throughput ops/sec, Mean Latency: $mean_latency ns" | tee -a $RESULTS_FILE
        fi
    else
        echo "  FAILED: Exit status $exit_status, Memory exceeded: $memory_exceeded" | tee -a $RESULTS_FILE
        failed_at=$events
        break
    fi
    
    echo "" | tee -a $RESULTS_FILE
    
    # Clean up
    rm -f temp_benchmark_output.log
    
    # Brief pause between tests
    sleep 2
done

echo "=== RESULTS ===" | tee -a $RESULTS_FILE
echo "Maximum successful event count: $successful_max" | tee -a $RESULTS_FILE
if [ $failed_at -gt 0 ]; then
    echo "Failed at event count: $failed_at" | tee -a $RESULTS_FILE
fi

# Suggest optimal event count (80% of max successful)
optimal=$(echo "$successful_max * 0.8" | bc | cut -d. -f1)
echo "Recommended safe event count: $optimal (80% of maximum)" | tee -a $RESULTS_FILE

echo "" | tee -a $RESULTS_FILE
echo "Test completed. Results saved to: $RESULTS_FILE"