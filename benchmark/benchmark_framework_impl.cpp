#include <algorithm>
#include <numeric>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

#include "benchmark_framework.h"
#include "../src/lob_engine.h"

namespace BenchmarkFramework {

// Explicit template instantiation for LOBEngine
template class BenchmarkRunner<LOBEngine>;

template<>
PerformanceStats BenchmarkRunner<LOBEngine>::run_latency_benchmark(size_t num_operations, size_t warmup_ops) {
    std::vector<double> latencies;
    latencies.reserve(num_operations);
    
    MemoryTracker memory_tracker;
    HighResTimer timer;
    PerfCounterTracker perf_tracker;
    
    // Initialize performance counters
    if (!perf_tracker.init()) {
        std::cout << "Warning: Performance counters not available, will report 0 for hardware metrics" << std::endl;
    }
    
    // Load batch of market data (similar to throughput test approach)
    size_t batch_size = std::min(50000UL, data_loader.get_total_available_events());
    auto orders = data_loader.load_order_sequence(batch_size);
    
    if (orders.empty()) {
        throw std::runtime_error("No market data available for benchmarking");
    }
    
    std::cout << "Loaded " << orders.size() << " order events for cycling" << std::endl;
    
    // Warmup phase with cycling
    size_t order_index = 0;
    size_t cycle_count = 0;
    
    for (size_t warmup_op = 0; warmup_op < warmup_ops; ++warmup_op) {
        // Cycle back to beginning when we reach the end
        if (order_index >= orders.size()) {
            order_index = 0;
            cycle_count++;
            // Every few cycles, reset parser and reload fresh data
            if (cycle_count % 2 == 0) {
                data_loader.reset_parser();
                orders = data_loader.load_order_sequence(batch_size);
                if (orders.empty()) break;
            }
        }
        
        const auto& order = orders[order_index];
        // Convert to OrderMessage format used by LOBEngine
        OrderMessage msg;
        msg.order_id = order.order_id;
        msg.price = order.price;
        msg.quantity = order.quantity;
        msg.timestamp = order.timestamp_ns;
        
        switch (order.action) {
            case OrderEvent::ADD:
                msg.msg_type = MessageType::ADD_ORDER;
                msg.side = (order.side == OrderEvent::BUY) ? Side::BUY : Side::SELL;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::CANCEL:
                msg.msg_type = MessageType::CANCEL_ORDER;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::MODIFY:
                msg.msg_type = MessageType::MODIFY_ORDER;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::EXECUTE:
                // Executions are handled internally by the engine during matching
                break;
        }
        
        order_index++;
    }
    
    // Flush caches to ensure clean benchmark environment
    CacheFlusher::flush_all_caches();
    
    perf_tracker.start_counting();
    timer.start();
    order_index = 0;
    cycle_count = 0;
    
    for (size_t op = 0; op < num_operations; ++op) {
        if (order_index >= orders.size()) {
            order_index = 0;
            cycle_count++;
            if (cycle_count % 2 == 0) {
                data_loader.reset_parser();
                orders = data_loader.load_order_sequence(batch_size);
                if (orders.empty()) break;
            }
        }
        
        const auto& order = orders[order_index];
        
        HighResTimer op_timer;
        op_timer.start();
        
        OrderMessage msg;
        msg.order_id = order.order_id;
        msg.price = order.price;
        msg.quantity = order.quantity;
        msg.timestamp = order.timestamp_ns;
        
        switch (order.action) {
            case OrderEvent::ADD:
                msg.msg_type = MessageType::ADD_ORDER;
                msg.side = (order.side == OrderEvent::BUY) ? Side::BUY : Side::SELL;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::CANCEL:
                msg.msg_type = MessageType::CANCEL_ORDER;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::MODIFY:
                msg.msg_type = MessageType::MODIFY_ORDER;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::EXECUTE:
                break;
        }
        
        latencies.push_back(op_timer.elapsed_ns());
        order_index++;
        
        if (op % 10000 == 0) {
            memory_tracker.update_peak();
        }
    }
    
    double total_time = timer.elapsed_sec();
    perf_tracker.stop_counting();
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    
    PerformanceStats stats;
    stats.total_operations = num_operations;
    stats.total_time_sec = total_time;
    stats.throughput_ops_per_sec = 0.0; // Latency test does not measure throughput
    stats.peak_memory_kb = memory_tracker.get_peak_usage_kb();
    
    // Latency percentiles
    stats.mean_latency_ns = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    stats.p50_latency_ns = latencies[latencies.size() * 0.50];
    stats.p95_latency_ns = latencies[latencies.size() * 0.95];
    stats.p99_latency_ns = latencies[latencies.size() * 0.99];
    stats.p99_9_latency_ns = latencies[latencies.size() * 0.999];
    
    // Calculate hardware performance metrics
    uint64_t cycles = perf_tracker.get_cpu_cycles();
    uint64_t instructions = perf_tracker.get_instructions();
    uint64_t l1_misses = perf_tracker.get_l1_cache_misses();
    uint64_t l1_accesses = perf_tracker.get_l1_cache_accesses();
    uint64_t l2_misses = perf_tracker.get_l2_cache_misses();
    uint64_t l2_accesses = perf_tracker.get_l2_cache_accesses();
    uint64_t l3_misses = perf_tracker.get_l3_cache_misses();
    uint64_t l3_accesses = perf_tracker.get_l3_cache_accesses();
    uint64_t branch_instr = perf_tracker.get_branch_instructions();
    uint64_t branch_misses = perf_tracker.get_branch_misses();
    uint64_t mem_loads = perf_tracker.get_memory_loads();
    uint64_t mem_stores = perf_tracker.get_memory_stores();
    
    stats.cpu_cycles_per_op = cycles > 0 ? (double)cycles / stats.total_operations : 0.0;
    stats.instructions_per_cycle = cycles > 0 ? (double)instructions / cycles : 0.0;
    stats.l1_cache_miss_rate = l1_accesses > 0 ? (double)l1_misses / l1_accesses : 0.0;
    stats.l2_cache_miss_rate = l2_accesses > 0 ? (double)l2_misses / l2_accesses : 0.0;
    stats.l3_cache_miss_rate = l3_accesses > 0 ? (double)l3_misses / l3_accesses : 0.0;
    stats.branch_misprediction_rate = branch_instr > 0 ? (double)branch_misses / branch_instr : 0.0;
    
    // Estimate memory bandwidth (assuming 64-byte cache line size)
    uint64_t total_memory_accesses = mem_loads + mem_stores;
    double total_bytes = total_memory_accesses * 64.0; // Approximate
    stats.memory_bandwidth_gb_per_sec = total_time > 0 ? (total_bytes / (1024*1024*1024)) / total_time : 0.0;
    
    return stats;
}

template<>
PerformanceStats BenchmarkRunner<LOBEngine>::run_throughput_benchmark(size_t num_operations) {
    MemoryTracker memory_tracker;
    HighResTimer timer;
    PerfCounterTracker perf_tracker;
    
    if (!perf_tracker.init()) {
        std::cout << "Warning: Performance counters not available, will report 0 for hardware metrics" << std::endl;
    }
    
    size_t operations_completed = 0;
    
    CacheFlusher::flush_all_caches();
    
    // Reset parser to ensure fresh start for throughput test
    data_loader.reset_parser();
    
    // Load reasonable batch of market data (smaller than total to enable cycling)
    size_t batch_size = std::min(50000UL, data_loader.get_total_available_events());
    auto orders = data_loader.load_order_sequence(batch_size);
    
    if (orders.empty()) {
        std::cerr << "Error: No order events loaded for throughput test" << std::endl;
        PerformanceStats empty_stats = {};
        return empty_stats;
    }
    
    perf_tracker.start_counting();
    timer.start();
    
    size_t order_index = 0;
    size_t cycle_count = 0;
    
    while (operations_completed < num_operations) {
        if (order_index >= orders.size()) {
            order_index = 0;
            cycle_count++;
            if (cycle_count % 2 == 0) {
                data_loader.reset_parser();
                orders = data_loader.load_order_sequence(batch_size);
                if (orders.empty()) break; // Safety check
            }
        }
        const auto& order = orders[order_index];
        
        OrderMessage msg;
        msg.order_id = order.order_id;
        msg.price = order.price;
        msg.quantity = order.quantity;
        msg.timestamp = order.timestamp_ns;
        
        switch (order.action) {
            case OrderEvent::ADD:
                msg.msg_type = MessageType::ADD_ORDER;
                msg.side = (order.side == OrderEvent::BUY) ? Side::BUY : Side::SELL;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::CANCEL:
                msg.msg_type = MessageType::CANCEL_ORDER;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::MODIFY:
                msg.msg_type = MessageType::MODIFY_ORDER;
                lob_engine.process_message(msg);
                break;
            case OrderEvent::EXECUTE:
                break;
        }
        
        operations_completed++;
        order_index++;
        
        if (operations_completed % 10000 == 0) {
            memory_tracker.update_peak();
        }
    }
    
    double actual_time = timer.elapsed_sec();
    perf_tracker.stop_counting();
    
    PerformanceStats stats;
    stats.total_operations = operations_completed;
    stats.total_time_sec = actual_time;
    stats.throughput_ops_per_sec = operations_completed / actual_time;
    stats.peak_memory_kb = memory_tracker.get_peak_usage_kb();
    
    uint64_t cycles = perf_tracker.get_cpu_cycles();
    uint64_t instructions = perf_tracker.get_instructions();
    uint64_t l1_misses = perf_tracker.get_l1_cache_misses();
    uint64_t l1_accesses = perf_tracker.get_l1_cache_accesses();
    uint64_t l2_misses = perf_tracker.get_l2_cache_misses();
    uint64_t l2_accesses = perf_tracker.get_l2_cache_accesses();
    uint64_t l3_misses = perf_tracker.get_l3_cache_misses();
    uint64_t l3_accesses = perf_tracker.get_l3_cache_accesses();
    uint64_t branch_instr = perf_tracker.get_branch_instructions();
    uint64_t branch_misses = perf_tracker.get_branch_misses();
    uint64_t mem_loads = perf_tracker.get_memory_loads();
    uint64_t mem_stores = perf_tracker.get_memory_stores();
    
    stats.cpu_cycles_per_op = cycles > 0 ? (double)cycles / stats.total_operations : 0.0;
    stats.instructions_per_cycle = cycles > 0 ? (double)instructions / cycles : 0.0;
    stats.l1_cache_miss_rate = l1_accesses > 0 ? (double)l1_misses / l1_accesses : 0.0;
    stats.l2_cache_miss_rate = l2_accesses > 0 ? (double)l2_misses / l2_accesses : 0.0;
    stats.l3_cache_miss_rate = l3_accesses > 0 ? (double)l3_misses / l3_accesses : 0.0;
    stats.branch_misprediction_rate = branch_instr > 0 ? (double)branch_misses / branch_instr : 0.0;
    
    uint64_t total_memory_accesses = mem_loads + mem_stores;
    double total_bytes = total_memory_accesses * 64.0; // Approximate
    stats.memory_bandwidth_gb_per_sec = actual_time > 0 ? (total_bytes / (1024*1024*1024)) / actual_time : 0.0;
    
    // Latency metrics are not measured in throughput test
    stats.mean_latency_ns = 0.0;
    stats.p50_latency_ns = 0.0;
    stats.p95_latency_ns = 0.0;
    stats.p99_latency_ns = 0.0;
    stats.p99_9_latency_ns = 0.0;
    
    return stats;
}

} 
