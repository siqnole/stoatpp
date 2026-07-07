#include "stoatpp/cluster.h"
#include "stoatpp/bot_module.h"
#include "stoatpp/utils/logger.h"
#include <thread>
#include <chrono>

namespace stoatpp {

cluster::cluster(const std::string& token, ClientConfig config)
    : token_(token),
      config_(config),
      rest_(token, config),
      dispatcher_(),
      gateway_(token, config, dispatcher_),
      launch_time_(std::chrono::steady_clock::now()) {

    rest_.set_error_callback([this](const std::string& method, const std::string& path,
                                    int status_code, const std::string& error_msg) {
        if (rest_error_handler_) {
            rest_error_handler_(method, path, status_code, error_msg);
        }
    });

    if (config_.enable_default_help) {
        setup_default_help();
    }

    setup_message_listener();
    setup_cache_listeners();
}

void cluster::start(bool return_after_init) {
    utils::logger::log(LogLevel::INFO, "Starting cluster...", config_);

    // Auto-discover Autumn URL from node info before connecting
    try {
        auto node = rest_.get_node_info();
        if (node.success() &&
            node.body.contains("features") &&
            node.body["features"].contains("autumn") &&
            node.body["features"]["autumn"].contains("url") &&
            node.body["features"]["autumn"]["url"].is_string()) {
            std::string autumn = node.body["features"]["autumn"]["url"].get<std::string>();
            config_.autumn_url = autumn;
            rest_.update_config(config_);
            utils::logger::log(LogLevel::INFO, "Autumn URL: " + autumn, config_);
        }
    } catch (...) {
        utils::logger::log(LogLevel::WARNING,
            "Could not fetch node info; using default Autumn URL", config_);
    }

    timers_running_ = true;
    timer_thread_ = std::thread(&cluster::run_timer_loop, this);

    gateway_.connect();

    running_ = true;
    if (return_after_init) return;

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void cluster::stop() {
    utils::logger::log(LogLevel::INFO, "Stopping cluster...", config_);
    running_ = false;

    timers_running_ = false;
    timer_cv_.notify_all();
    if (timer_thread_.joinable()) {
        if (std::this_thread::get_id() == timer_thread_.get_id()) {
            timer_thread_.detach();
        } else {
            timer_thread_.join();
        }
    }

    gateway_.disconnect();
}

} // namespace stoatpp
