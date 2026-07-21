#include "stoatpp/utils/logger.h"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace stoatpp::utils {

static std::mutex log_mutex;

void logger::log(LogLevel level, const std::string& message, const ClientConfig& config) {
    if (config.log_level == LogLevel::NONE || level > config.log_level) {
        return;
    }

    if (config.log_callback) {
        config.log_callback(level, message);
        return;
    }

    std::lock_guard<std::mutex> lock(log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    struct tm buf;
    localtime_r(&time_t_now, &buf);

    const char* lvl_str = "INFO";
    const char* color_code = "\033[1;32m";
    switch (level) {
        case LogLevel::ERROR:   lvl_str = "ERROR"; color_code = "\033[1;31m"; break;
        case LogLevel::WARNING: lvl_str = "WARN "; color_code = "\033[1;33m"; break;
        case LogLevel::INFO:    lvl_str = "INFO "; color_code = "\033[1;32m"; break;
        case LogLevel::DEBUG:   lvl_str = "DEBUG"; color_code = "\033[1;35m"; break;
        case LogLevel::TRACE:   lvl_str = "TRACE"; color_code = "\033[2;37m"; break;
        default: break;
    }

    std::ostream& out = (level == LogLevel::ERROR) ? std::cerr : std::cout;
    out << "\033[90m[" << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << "." 
        << std::setfill('0') << std::setw(3) << ms.count() << "]\033[0m "
        << "\033[1;36m[stoat++]\033[0m [" << color_code << lvl_str << "\033[0m] " << message << std::endl;
}

} // namespace stoatpp::utils
