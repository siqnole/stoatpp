#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "client_config.h"
#include "event_handler.h"

namespace stoatpp {

class gateway {
public:
    gateway(const std::string& token, const ClientConfig& config,
            event_dispatcher& dispatcher);
    ~gateway();

    void connect();
    void disconnect();
    bool is_connected() const;

    void send_raw(const nlohmann::json& payload);

    void authenticate(const std::string& token);
    void ping(int64_t data = 0);
    void begin_typing(const std::string& channel_id);
    void end_typing(const std::string& channel_id);
    void subscribe(const std::string& server_id);

private:
    void on_message_received(const std::string& raw);
    void on_connected();
    void on_disconnected();
    void schedule_ping();
    void handle_event(const nlohmann::json& j);

    struct impl;
    std::string token_;
    ClientConfig config_;
    event_dispatcher& dispatcher_;
    std::unique_ptr<impl> pimpl_;
};

} // namespace stoatpp
