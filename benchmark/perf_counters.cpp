#include "benchmark_framework.h"
#include <cstring>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <errno.h>

namespace BenchmarkFramework {

PerfCounterTracker::PerfCounterTracker() : initialized(false) {}

PerfCounterTracker::~PerfCounterTracker() {
    for (auto& counter : counters) {
        if (counter.fd >= 0) {
            close(counter.fd);
        }
    }
}

bool PerfCounterTracker::init() {
    if (initialized) return true;
    
    std::ifstream paranoid("/proc/sys/kernel/perf_event_paranoid");
    int paranoid_level = 3; // Default to most restrictive
    if (paranoid.is_open()) {
        paranoid >> paranoid_level;
        paranoid.close();
    }
    
    if (paranoid_level > 1) {
        std::cout << "Warning: perf_event_paranoid=" << paranoid_level 
                  << " restricts performance counters. Use 'sudo sysctl kernel.perf_event_paranoid=1' to enable." << std::endl;
        // Still try to initialize with reduced functionality
    }
    
    struct {
        uint32_t type;
        uint64_t config;
        const char* name;
    } counter_configs[] = {
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cpu_cycles"},
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions"},
        {PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), "l1d_accesses"},
        {PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), "l1d_misses"},
        {PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), "ll_accesses"},
        {PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), "ll_misses"},
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "branch_instructions"},
        {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES, "branch_misses"}
    };
    
    bool any_success = false;
    
    for (auto& config : counter_configs) {
        int fd = create_counter(config.type, config.config);
        if (fd >= 0) {
            counters.push_back({fd, config.type, config.config, config.name});
            any_success = true;
        } else {
            // Print specific error for debugging
            if (errno == EACCES) {
                std::cout << "Permission denied for counter: " << config.name << std::endl;
            } else if (errno == ENOENT) {
                std::cout << "Counter not supported: " << config.name << std::endl;
            }
            // Still add entry with -1 fd to maintain consistent indexing
            counters.push_back({-1, config.type, config.config, config.name});
        }
    }
    
    initialized = any_success;
    if (!any_success) {
        std::cout << "No performance counters available. Run with 'sudo sysctl kernel.perf_event_paranoid=1' for hardware metrics." << std::endl;
    }
    return initialized;
}

void PerfCounterTracker::start_counting() {
    for (auto& counter : counters) {
        if (counter.fd >= 0) {
            ioctl(counter.fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(counter.fd, PERF_EVENT_IOC_ENABLE, 0);
        }
    }
}

void PerfCounterTracker::stop_counting() {
    for (auto& counter : counters) {
        if (counter.fd >= 0) {
            ioctl(counter.fd, PERF_EVENT_IOC_DISABLE, 0);
        }
    }
}

uint64_t PerfCounterTracker::get_cpu_cycles() const {
    return counters.size() > 0 ? read_counter(counters[0].fd) : 0;
}

uint64_t PerfCounterTracker::get_instructions() const {
    return counters.size() > 1 ? read_counter(counters[1].fd) : 0;
}

uint64_t PerfCounterTracker::get_l1_cache_accesses() const {
    return counters.size() > 2 ? read_counter(counters[2].fd) : 0;
}

uint64_t PerfCounterTracker::get_l1_cache_misses() const {
    return counters.size() > 3 ? read_counter(counters[3].fd) : 0;
}

uint64_t PerfCounterTracker::get_l2_cache_accesses() const {
    // Approximate L2 as subset of L1 misses for simplicity
    return get_l1_cache_misses();
}

uint64_t PerfCounterTracker::get_l2_cache_misses() const {
    // Approximate L2 misses as subset of L3 accesses
    return get_l3_cache_accesses() / 2;
}

uint64_t PerfCounterTracker::get_l3_cache_accesses() const {
    return counters.size() > 4 ? read_counter(counters[4].fd) : 0;
}

uint64_t PerfCounterTracker::get_l3_cache_misses() const {
    return counters.size() > 5 ? read_counter(counters[5].fd) : 0;
}

uint64_t PerfCounterTracker::get_branch_instructions() const {
    return counters.size() > 6 ? read_counter(counters[6].fd) : 0;
}

uint64_t PerfCounterTracker::get_branch_misses() const {
    return counters.size() > 7 ? read_counter(counters[7].fd) : 0;
}

uint64_t PerfCounterTracker::get_memory_loads() const {
    // Approximate as L1 cache accesses
    return get_l1_cache_accesses();
}

uint64_t PerfCounterTracker::get_memory_stores() const {
    // Approximate as fraction of L1 accesses (stores typically ~30% of total)
    return get_l1_cache_accesses() / 3;
}

int PerfCounterTracker::create_counter(uint32_t type, uint64_t config) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    
    pe.type = type;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = config;
    pe.disabled = 1;
    pe.exclude_kernel = 1;  // Exclude kernel events (more likely to work without root)
    pe.exclude_hv = 1;      // Exclude hypervisor
    pe.exclude_idle = 1;    // Don't count when idle
    
    int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd < 0) {
        // Try with more permissive settings
        pe.exclude_kernel = 1;
        pe.exclude_user = 0;
        fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    }
    
    return fd;
}

uint64_t PerfCounterTracker::read_counter(int fd) const {
    if (fd < 0) return 0;
    
    uint64_t value = 0;
    ssize_t bytes_read = read(fd, &value, sizeof(value));
    if (bytes_read != sizeof(value)) {
        return 0;
    }
    return value;
}

}
