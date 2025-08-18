#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

#include "benchmark_framework.h"
#include "nasdaq_itch_parser.h"
#include "../src/lob_engine.h"

namespace BenchmarkFramework {

void PerformanceStats::to_csv(std::ostream& os, const std::string& config_name) const {
    os << config_name << ","
       << total_operations << ","
       << total_time_sec << ","
       << throughput_ops_per_sec << ","
       << mean_latency_ns << ","
       << p50_latency_ns << ","
       << p95_latency_ns << ","
       << p99_latency_ns << ","
       << p99_9_latency_ns << ","
       << peak_memory_kb << ","
       << cpu_cycles_per_op << ","
       << instructions_per_cycle << ","
       << l1_cache_miss_rate << ","
       << l2_cache_miss_rate << ","
       << l3_cache_miss_rate << ","
       << memory_bandwidth_gb_per_sec << ","
       << branch_misprediction_rate << "\n";
}

void PerformanceStats::print_summary() const {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== Performance Summary ===" << std::endl;
    std::cout << "Total Operations: " << total_operations << std::endl;
    std::cout << "Total Time: " << total_time_sec << " seconds" << std::endl;
    std::cout << "Throughput: " << throughput_ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Peak Memory: " << peak_memory_kb << " KB" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Latency Distribution:" << std::endl;
    std::cout << "  Mean: " << mean_latency_ns << " ns" << std::endl;
    std::cout << "  P50:  " << p50_latency_ns << " ns" << std::endl;
    std::cout << "  P95:  " << p95_latency_ns << " ns" << std::endl;
    std::cout << "  P99:  " << p99_latency_ns << " ns" << std::endl;
    std::cout << "  P99.9:" << p99_9_latency_ns << " ns" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Hardware Metrics:" << std::endl;
    std::cout << "  CPU Cycles/Op: " << cpu_cycles_per_op << std::endl;
    std::cout << "  Instructions/Cycle: " << instructions_per_cycle << std::endl;
    std::cout << "  L1 Cache Miss Rate: " << (l1_cache_miss_rate * 100) << "%" << std::endl;
    std::cout << "  L2 Cache Miss Rate: " << (l2_cache_miss_rate * 100) << "%" << std::endl;
    std::cout << "  L3 Cache Miss Rate: " << (l3_cache_miss_rate * 100) << "%" << std::endl;
    std::cout << "  Memory Bandwidth: " << memory_bandwidth_gb_per_sec << " GB/s" << std::endl;
    std::cout << "  Branch Misprediction: " << (branch_misprediction_rate * 100) << "%" << std::endl;
    std::cout << std::endl;
}

RealMarketDataLoader::RealMarketDataLoader(const std::string& file_path, 
  const std::string& symbol, size_t max_events): 
  data_file_path(file_path), target_symbol(symbol), max_events_per_test(max_events) {
    
    std::cout << "Initializing real market data loader..." << std::endl;
    std::cout << "Data file: " << data_file_path << std::endl;
    std::cout << "Max events per test: " << max_events_per_test << std::endl;
    if (!target_symbol.empty()) {
        std::cout << "Symbol filter: " << target_symbol << std::endl;
    }
}

std::vector<OrderEvent> RealMarketDataLoader::load_order_sequence(size_t max_count) {
    // Use the smaller of requested count or configured max events per test
    size_t actual_max = std::min(max_count, max_events_per_test);
    std::vector<OrderEvent> events;
    events.reserve(actual_max);
    
    try {
        ensure_parser_initialized();
        
        ::NasdaqItch::BenchmarkOrderEvent itch_event;
        size_t loaded = 0;
        
        while (loaded < actual_max && persistent_parser->get_next_order_event(itch_event)) {
            // Apply symbol filter if specified
            if (!target_symbol.empty() && itch_event.symbol != target_symbol) {
                continue;
            }
            
            // Convert ITCH event to benchmark event
            OrderEvent event;
            event.order_id = itch_event.order_id;
            event.price = itch_event.price;
            event.quantity = itch_event.quantity;
            event.timestamp_ns = itch_event.timestamp_ns;
            event.symbol = itch_event.symbol;
            
            switch (itch_event.action) {
                case ::NasdaqItch::BenchmarkOrderEvent::ADD:
                    event.action = OrderEvent::ADD;
                    break;
                case ::NasdaqItch::BenchmarkOrderEvent::CANCEL:
                    event.action = OrderEvent::CANCEL;
                    break;
                case ::NasdaqItch::BenchmarkOrderEvent::MODIFY:
                    event.action = OrderEvent::MODIFY;
                    break;
                case ::NasdaqItch::BenchmarkOrderEvent::EXECUTE:
                    event.action = OrderEvent::EXECUTE;
                    break;
            }
            
            event.side = (itch_event.side == ::NasdaqItch::BenchmarkOrderEvent::BUY) ? 
                        OrderEvent::BUY : OrderEvent::SELL;
            
            events.push_back(event);
            loaded++;
        }
        
        std::cout << "Loaded " << loaded << " order events from ITCH data" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading ITCH data: " << e.what() << std::endl;
        throw;
    }
    
    return events;
}

void RealMarketDataLoader::print_data_statistics() {
    try {
        ::NasdaqItch::ItchParser parser(data_file_path);
        auto stats = parser.get_file_statistics();
        
        std::cout << "\n=== NASDAQ ITCH Data Statistics ===" << std::endl;
        std::cout << "File: " << data_file_path << std::endl;
        std::cout << "Total messages: " << stats.total_messages << std::endl;
        std::cout << "Add orders: " << stats.add_orders << std::endl;
        std::cout << "Cancellations: " << stats.cancellations << std::endl;
        std::cout << "Executions: " << stats.executions << std::endl;
        std::cout << "Unique symbols: " << stats.unique_symbols << std::endl;
        std::cout << "Time span: " << stats.time_span_ns / 1e9 << " seconds" << std::endl;
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error getting statistics: " << e.what() << std::endl;
    }
}

size_t RealMarketDataLoader::get_total_available_events() {
    try {
        ::NasdaqItch::ItchParser parser(data_file_path);
        auto stats = parser.get_file_statistics();
        return stats.add_orders + stats.cancellations + stats.executions;
    } catch (const std::exception& e) {
        std::cerr << "Error getting event count: " << e.what() << std::endl;
        return 0;
    }
}

void RealMarketDataLoader::set_symbol_filter(const std::string& symbol) {
    target_symbol = symbol;
    std::cout << "Updated symbol filter to: " << (symbol.empty() ? "ALL" : symbol) << std::endl;
}

// Template instantiation for save_results_csv with LOBEngine
template<>
void BenchmarkRunner<LOBEngine>::save_results_csv(const std::vector<PerformanceStats>& results, const std::string& filename) {
    std::ofstream file(filename);
    file << "test_type,config,total_ops,total_time_sec,throughput_ops_per_sec,mean_latency_ns,p50_latency_ns,p95_latency_ns,p99_latency_ns,p99_9_latency_ns,peak_memory_kb,cpu_cycles_per_op,instructions_per_cycle,l1_cache_miss_rate,l2_cache_miss_rate,l3_cache_miss_rate,memory_bandwidth_gb_per_sec,branch_misprediction_rate\n";
    
    const char* test_types[] = {"latency", "throughput"};
    for (size_t i = 0; i < results.size() && i < 2; ++i) {
        file << test_types[i] << ",";
        results[i].to_csv(file, config_name);
    }
}

void RealMarketDataLoader::ensure_parser_initialized() {
    if (!persistent_parser) {
        persistent_parser = std::make_unique<::NasdaqItch::ItchParser>(data_file_path);
    }
}

void RealMarketDataLoader::reset_parser() {
    if (persistent_parser) {
        persistent_parser->reset();
    }
}

} 
