#include "stoatpp/gateway.h"

namespace stoatpp {

struct gateway::impl {
    // Stub structure for Pimpl pattern
};

gateway::gateway(const std::string& token, const ClientConfig& config,
                 event_dispatcher& dispatcher)
    : token_(token), config_(config), dispatcher_(dispatcher), pimpl_(std::make_unique<impl>()) {}

gateway::~gateway() = default;

void gateway::connect() {
    // Stub
}

void gateway::disconnect() {
    // Stub
}

bool gateway::is_connected() const {
    return false;
}

void gateway::send_raw(const nlohmann::json& payload) {
    // Stub
}

void gateway::authenticate(const std::string& token) {
    // Stub
}

void gateway::ping(int64_t data) {
    // Stub
}

void gateway::begin_typing(const std::string& channel_id) {
    // Stub
}

void gateway::end_typing(const std::string& channel_id) {
    // Stub
}

void gateway::subscribe(const std::string& server_id) {
    // Stub
}

void gateway::on_message_received(const std::string& raw) {
    // Stub
}

void gateway::on_connected() {
    // Stub
}

void gateway::on_disconnected() {
    // Stub
}

void gateway::schedule_ping() {
    // Stub
}

void gateway::handle_event(const nlohmann::json& j) {
    // Stub
}

} // namespace stoatpp
