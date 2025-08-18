#include "nasdaq_itch_parser.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string data_file_path = "data/01302019.NASDAQ_ITCH50";
    
    // Check if custom path provided
    if (argc > 1) {
        data_file_path = argv[1];
    }
    
    std::cout << "Analyzing ITCH file: " << data_file_path << std::endl;
    std::cout << "=======================================" << std::endl;
    
    try {
        NasdaqItch::ItchParser parser(data_file_path);
        auto stats = parser.get_file_statistics();
        
        std::cout << "NASDAQ ITCH Data Statistics:" << std::endl;
        std::cout << "Total messages: " << stats.total_messages << std::endl;
        std::cout << "Add orders: " << stats.add_orders << std::endl;
        std::cout << "Cancellations: " << stats.cancellations << std::endl;
        std::cout << "Executions: " << stats.executions << std::endl;
        std::cout << "Unique symbols: " << stats.unique_symbols << std::endl;
        std::cout << "Time span: " << stats.time_span_ns / 1e9 << " seconds" << std::endl;
        
        // Calculate total order events
        size_t total_order_events = stats.add_orders + stats.cancellations + stats.executions;
        std::cout << std::endl;
        std::cout << "Total order events: " << total_order_events << std::endl;
        std::cout << "Non-order messages: " << (stats.total_messages - total_order_events) << std::endl;
        std::cout << "Order event percentage: " << (100.0 * total_order_events / stats.total_messages) << "%" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
