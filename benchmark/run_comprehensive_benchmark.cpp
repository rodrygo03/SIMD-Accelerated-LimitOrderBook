#include "benchmark_framework.h"
#include "../src/lob_engine.h"
#include "../src/optimization_config.h"
#ifdef USE_CMAKE_CONFIG
#include "benchmark_config.h"
#endif
#include <iostream>
#include <vector>
#include <memory>
#include <fstream>
#include <cstdlib>
#include <map>

using namespace BenchmarkFramework;
using namespace OptimizationConfig;

// Configuration value reader - checks environment variables first, then CMake defaults
std::string get_config_value(const std::string& env_name, const std::string& cmake_default) {
    // First check actual environment variables (highest priority)
    const char* env_value = std::getenv(env_name.c_str());
    if (env_value) return std::string(env_value);
    
    // Use CMake-configured default
    return cmake_default;
}

// Run configuration benchmark (LOBEngine is not templated in this implementation)
void run_configuration_benchmark(const std::string& config_name, const std::string& output_dir, 
                                const std::string& data_file_path, const std::string& symbol_filter = "", 
                                size_t max_events = 16227, bool verbose = false) {
    std::cout << "\n=== Running benchmarks for " << config_name << " ===" << std::endl;
    
    // Check if system cache clearing is enabled
    const char* clear_caches_env = std::getenv("CLEAR_SYSTEM_CACHES");
    bool clear_system_caches = (clear_caches_env && std::string(clear_caches_env) == "true");
    
    if (clear_system_caches) {
        std::cout << "System cache clearing enabled via environment variable" << std::endl;
        // System cache clearing is handled by the shell script
        // CPU cache clearing is handled automatically by the C++ benchmark framework
    } else {
        std::cout << "Note: Set CLEAR_SYSTEM_CACHES=true for complete cache isolation" << std::endl;
    }
    
    std::cout << "Compiled configuration:" << std::endl;
    std::cout << "  Template config: " << ConfigDebugInfo<DefaultConfig>::get_config_name() << std::endl;
    ConfigDebugInfo<DefaultConfig>::print_config();
    std::cout << "  Runtime config: " << config_name << std::endl;
    std::cout << std::endl;
    
    LOBEngine lob_engine;
    
    std::cout << "Single file mode: " << data_file_path << std::endl;
    auto runner = std::make_unique<BenchmarkRunner<LOBEngine>>(config_name, lob_engine, data_file_path, symbol_filter, max_events);
    
    std::vector<PerformanceStats> all_results;
    
    // 1. Latency Benchmark - configurable operations with CMake-configured warmup
#ifdef USE_CMAKE_CONFIG
    double warmup_ratio = std::stod(get_config_value("WARMUP_RATIO", BenchmarkConfig::WARMUP_RATIO));
    size_t min_warmup = std::stoull(get_config_value("MIN_WARMUP_EVENTS", BenchmarkConfig::MIN_WARMUP_EVENTS));
    size_t max_warmup = std::stoull(get_config_value("MAX_WARMUP_EVENTS", BenchmarkConfig::MAX_WARMUP_EVENTS));
#else
    double warmup_ratio = 0.1;
    size_t min_warmup = 10;
    size_t max_warmup = 10000;
#endif
    size_t warmup_ops = std::max(min_warmup, std::min(max_warmup, (size_t)(max_events * warmup_ratio)));
    
    // Ensure warmup doesn't exceed available events
    warmup_ops = std::min(warmup_ops, max_events / 2);
    
    if (verbose) {
        std::cout << "Warmup: " << warmup_ops << " events (" << (warmup_ratio * 100) << "% of total)" << std::endl;
    }
    std::cout << "Running latency benchmark (" << max_events << " ops)..." << std::flush;
    auto latency_stats = runner->run_latency_benchmark(max_events, warmup_ops);
    latency_stats.print_summary();
    all_results.push_back(latency_stats);
    
    // 2. Throughput Benchmark - use same number of operations as latency test
    std::cout << "Running throughput benchmark (" << max_events << " ops)..." << std::flush;
    auto throughput_stats = runner->run_throughput_benchmark(max_events);
    std::cout << "Throughput: " << throughput_stats.throughput_ops_per_sec << " ops/sec" << std::endl;
    all_results.push_back(throughput_stats);
    
    // Skip memory benchmark for now - not implemented
    std::cout << "Skipping memory benchmark (not implemented)" << std::endl;
    
    // Create summary CSV for this configuration using format: <config>_<events>_<datafile>
    // Extract just the filename from the full path
    std::string data_filename = data_file_path.substr(data_file_path.find_last_of("/") + 1);
    std::string summary_filename = output_dir + "/" + config_name + "_" + std::to_string(max_events) + "_" + data_filename + ".csv";
    std::ofstream summary_file(summary_filename);
    summary_file << "test_type,config,total_ops,total_time_sec,throughput_ops_per_sec,mean_latency_ns,p50_latency_ns,p95_latency_ns,p99_latency_ns,p99_9_latency_ns,peak_memory_kb,cpu_cycles_per_op,instructions_per_cycle,l1_cache_miss_rate,l2_cache_miss_rate,l3_cache_miss_rate,memory_bandwidth_gb_per_sec,branch_misprediction_rate\n";
    
    const char* test_types[] = {"latency", "throughput"};
    for (size_t i = 0; i < all_results.size(); ++i) {
        summary_file << test_types[i] << ",";
        all_results[i].to_csv(summary_file, config_name);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "SIMD-LOB Comprehensive Benchmark Suite" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    std::string cmd_config = "";
    std::string cmd_max_events = "";
    
    if (argc >= 2) cmd_config = argv[1];
    if (argc >= 3) cmd_max_events = argv[2];
    
    // Read configuration (environment variables override CMake defaults, command line overrides environment)
#ifdef USE_CMAKE_CONFIG
    std::string single_config = !cmd_config.empty() ? cmd_config : get_config_value("BENCHMARK_CONFIG", "");
    std::string data_file_path = get_config_value("ITCH_DATA_FILE", BenchmarkConfig::ITCH_DATA_FILE);
    std::string symbol_filter = get_config_value("SYMBOL_FILTER", BenchmarkConfig::SYMBOL_FILTER);
    std::string output_dir = get_config_value("RESULTS_DIR", BenchmarkConfig::RESULTS_DIR);
    size_t max_events = !cmd_max_events.empty() ? std::stoull(cmd_max_events) : std::stoull(get_config_value("MAX_EVENTS_PER_TEST", BenchmarkConfig::MAX_EVENTS_PER_TEST));
    bool verbose = get_config_value("VERBOSE_OUTPUT", BenchmarkConfig::VERBOSE_OUTPUT) == "true";
    std::cout << "Using CMake configuration with environment variable overrides" << std::endl;
#else
    std::string single_config = !cmd_config.empty() ? cmd_config : "";
    std::string data_file_path = "benchmark/data/01302019.NASDAQ_ITCH50";
    std::string symbol_filter = "";
    std::string output_dir = "benchmark_results";
    size_t max_events = !cmd_max_events.empty() ? std::stoull(cmd_max_events) : 96000;  // Default updated for multi-file support
    bool verbose = false;
    std::cout << "Using hardcoded defaults (CMake config not available)" << std::endl;
#endif
    
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Mode: Single file" << std::endl;
    std::cout << "  Data file: " << data_file_path << std::endl;
    std::cout << "  Symbol filter: " << (symbol_filter.empty() ? "ALL" : symbol_filter) << std::endl;
    std::cout << "  Results directory: " << output_dir << std::endl;
    std::cout << "  Max events per test: " << max_events << std::endl;
    std::cout << "  Verbose output: " << (verbose ? "enabled" : "disabled") << std::endl;
    if (!single_config.empty()) {
        std::cout << "  Single config mode: " << single_config << std::endl;
    }
    
    std::ifstream data_check(data_file_path);
    if (!data_check.is_open()) {
        std::cerr << "\nError: NASDAQ ITCH data file not found: " << data_file_path << std::endl;
        std::cerr << "Please download the data file using:" << std::endl;
        std::cerr << "  mkdir -p benchmarks/data" << std::endl;
        std::cerr << "  curl -o benchmarks/data/01302019.NASDAQ_ITCH50.gz" << std::endl;
        std::cerr << "       \"ftp://emi.nasdaq.com/ITCH/01302019.NASDAQ_ITCH50.gz\"" << std::endl;
        std::cerr << "  gunzip benchmarks/data/01302019.NASDAQ_ITCH50.gz" << std::endl;
        return 1;
    }
    data_check.close();
    
    system(("mkdir -p " + output_dir).c_str());
    
    try {
        // Run benchmarks - either single config or all configurations
        std::vector<std::string> config_names;
        
        if (!single_config.empty()) {
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark(single_config, output_dir, data_file_path, symbol_filter, max_events, verbose);
            config_names.push_back(single_config);
        } else {
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("scalar-baseline", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("simd-baseline", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("object-pool-only", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("object-pool-simd", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("cache-only", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("memory-optimized", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            std::cout << "\n" << std::string(50, '=') << std::endl;
            run_configuration_benchmark("fully-optimized", output_dir, data_file_path, symbol_filter, max_events, verbose);
            
            config_names = {
                "scalar-baseline", "simd-baseline", "object-pool-only", "object-pool-simd",
                "cache-only", "memory-optimized", "fully-optimized"
            };
        }
        
        std::cout << "\n=== BENCHMARK COMPLETE ===" << std::endl;
        std::cout << "Results saved to: " << output_dir << "/" << std::endl;
        std::cout << "\nRun Python visualization script to generate charts:" << std::endl;
        std::cout << "python3 visualize_results.py " << output_dir << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

