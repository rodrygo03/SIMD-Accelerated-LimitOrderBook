#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <memory>
#include <random>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <sys/resource.h>
#include <unistd.h>

#ifdef USE_CMAKE_CONFIG
#include "benchmark_config.h"
#endif
#include "nasdaq_itch_parser.h"
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <asm/unistd.h>

namespace BenchmarkFramework {

struct PerformanceStats {
    double mean_latency_ns;
    double p50_latency_ns;
    double p95_latency_ns;
    double p99_latency_ns;
    double p99_9_latency_ns;
    double throughput_ops_per_sec;
    size_t peak_memory_kb;
    size_t total_operations;
    double total_time_sec;
    
    // CPU metrics
    double cpu_cycles_per_op;
    double instructions_per_cycle;
    
    // Cache metrics
    double l1_cache_miss_rate;
    double l2_cache_miss_rate;
    double l3_cache_miss_rate;
    
    // Memory and branch metrics
    double memory_bandwidth_gb_per_sec;
    double branch_misprediction_rate;
    
    void to_csv(std::ostream& os, const std::string& config_name) const;
    void print_summary() const;
};

class HighResTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed_ns() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    }
    
    double elapsed_us() const {
        return elapsed_ns() / 1000.0;
    }
    
    double elapsed_ms() const {
        return elapsed_ns() / 1000000.0;
    }
    
    double elapsed_sec() const {
        return elapsed_ns() / 1000000000.0;
    }
};

class MemoryTracker {
private:
    size_t initial_rss_kb;
    size_t peak_rss_kb;
    
public:
    MemoryTracker() : initial_rss_kb(get_current_rss_kb()), peak_rss_kb(initial_rss_kb) {}
    
    void update_peak() {
        size_t current = get_current_rss_kb();
        if (current > peak_rss_kb) {
            peak_rss_kb = current;
        }
    }
    
    size_t get_peak_usage_kb() const { return peak_rss_kb - initial_rss_kb; }
    
private:
    size_t get_current_rss_kb() const {
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                return std::stoul(line.substr(7)) * 1024; // Convert to bytes, then KB
            }
        }
        return 0;
    }
};

// Performance counter tracker using Linux perf_event
class PerfCounterTracker {
private:
    struct CounterInfo {
        int fd;
        uint64_t type;
        uint64_t config;
        const char* name;
    };
    
    std::vector<CounterInfo> counters;
    bool initialized;
    
public:
    PerfCounterTracker();
    ~PerfCounterTracker();
    
    bool init();
    void start_counting();
    void stop_counting();
    
    uint64_t get_cpu_cycles() const;
    uint64_t get_instructions() const;
    uint64_t get_l1_cache_misses() const;
    uint64_t get_l2_cache_misses() const;
    uint64_t get_l3_cache_misses() const;
    uint64_t get_l1_cache_accesses() const;
    uint64_t get_l2_cache_accesses() const;
    uint64_t get_l3_cache_accesses() const;
    uint64_t get_branch_instructions() const;
    uint64_t get_branch_misses() const;
    uint64_t get_memory_loads() const;
    uint64_t get_memory_stores() const;
    
private:
    int create_counter(uint32_t type, uint64_t config);
    uint64_t read_counter(int fd) const;
};

// Cache flushing utilities to prevent cache pollution between benchmark runs
class CacheFlusher {
public:
    // Flush CPU caches (L1/L2/L3) by allocating and touching large memory region
    static void flush_cpu_caches();
    
    // Flush memory hierarchy including TLB and branch predictors
    static void flush_memory_hierarchy();
    
    // Combined cache and memory flushing for maximum isolation
    static void flush_all_caches();
    
private:
    static constexpr size_t L3_CACHE_SIZE = 64 * 1024 * 1024; // 64MB - larger than typical L3
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t TLB_FLUSH_SIZE = 256 * 1024 * 1024; // 256MB for TLB flushing
};

// Configuration helpers
namespace BenchmarkDefaults {
    inline size_t get_default_max_events() {
#ifdef USE_CMAKE_CONFIG
        return std::stoull(BenchmarkConfig::MAX_EVENTS_PER_TEST);
#else
        return 16227;  // Fallback for single ITCH file
#endif
    }
}

class RealMarketDataLoader {
private:
    std::string data_file_path;  // Single file only
    std::string target_symbol;
    size_t max_events_per_test;
    
    // Persistent parser to maintain state between calls
    std::unique_ptr<::NasdaqItch::ItchParser> persistent_parser;
    
public:
    RealMarketDataLoader(const std::string& file_path, 
                        const std::string& symbol = "", 
                        size_t max_events = BenchmarkDefaults::get_default_max_events());
    
    std::vector<struct OrderEvent> load_order_sequence(size_t max_count);
    void print_data_statistics();
    
    // Set target symbol filter (empty = all symbols)
    void set_symbol_filter(const std::string& symbol);
    
    size_t get_total_available_events();
    
    // Reset parser to beginning (for throughput tests that need to cycle)
    void reset_parser();
    
private:
    void ensure_parser_initialized();
};

// Order event structure compatible with ITCH parser
struct OrderEvent {
    enum Action { ADD, CANCEL, MODIFY, EXECUTE };
    enum Side { BUY, SELL };
    
    Action action;
    Side side;
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    uint64_t timestamp_ns;
    std::string symbol;
};

template<typename LOBEngine>
class BenchmarkRunner {
private:
    std::string config_name;
    LOBEngine& lob_engine;
    RealMarketDataLoader data_loader;
    
public:
    BenchmarkRunner(const std::string& name, LOBEngine& engine, 
                   const std::string& data_file_path, 
                   const std::string& symbol_filter = "",
                   size_t max_events = BenchmarkDefaults::get_default_max_events()) 
        : config_name(name), lob_engine(engine), 
          data_loader(data_file_path, symbol_filter, max_events) {}
    
    // Core benchmark: measure latency distribution for different operations
    PerformanceStats run_latency_benchmark(size_t num_operations, size_t warmup_ops = 10000);
    
    // Throughput benchmark: total operations / total execution time
    PerformanceStats run_throughput_benchmark(size_t num_operations);
    
    void save_results_csv(const std::vector<PerformanceStats>& results, const std::string& filename);
};

}
