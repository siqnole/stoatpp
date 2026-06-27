#pragma once
#include <string>
#include "../client_config.h"

namespace stoatpp::utils {

class logger {
public:
    static void log(LogLevel level, const std::string& message, const ClientConfig& config);
};

} // namespace stoatpp::utils
