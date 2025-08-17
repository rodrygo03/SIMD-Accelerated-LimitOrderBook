#pragma once

#include "lob_engine.h"
#include "optimization_config.h"
#include <string>
#include <memory>
#include <stdexcept>

namespace LOBEngineFactory {

using namespace OptimizationConfig;

// Factory function to create LOBEngine with the correct configuration
// This works around the issue that LOBEngine uses DefaultConfig at compile time
std::unique_ptr<LOBEngine> createEngine(const std::string& config_name, size_t pool_size = 1000000);

std::vector<std::string> getAvailableConfigs();
std::string getConfigDescription(const std::string& config_name);
bool isValidConfig(const std::string& config_name);

// Print configuration info for debugging
void printConfigInfo(const std::string& config_name);

}
