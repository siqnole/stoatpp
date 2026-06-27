#include "stoatpp/cluster.h"

namespace stoatpp {

cluster::cluster(const std::string& token, ClientConfig config)
    : token_(token),
      config_(config),
      rest_(token, config),
      dispatcher_(),
      gateway_(token, config, dispatcher_) {}

void cluster::start(bool return_after_init) {
    // Stub
}

void cluster::stop() {
    // Stub
}

void cluster::on_ready(std::function<void(const events::Ready&)> cb) {
    dispatcher_.on_ready(cb);
}

void cluster::on_message(std::function<void(const events::Message&)> cb) {
    dispatcher_.on_message(cb);
}

void cluster::on_message_update(std::function<void(const events::MessageUpdate&)> cb) {
    dispatcher_.on_message_update(cb);
}

void cluster::on_message_delete(std::function<void(const events::MessageDelete&)> cb) {
    dispatcher_.on_message_delete(cb);
}

void cluster::on_message_react(std::function<void(const events::MessageReact&)> cb) {
    dispatcher_.on_message_react(cb);
}

void cluster::on_message_unreact(std::function<void(const events::MessageUnreact&)> cb) {
    dispatcher_.on_message_unreact(cb);
}

void cluster::on_channel_create(std::function<void(const events::ChannelCreate&)> cb) {
    dispatcher_.on_channel_create(cb);
}

void cluster::on_channel_update(std::function<void(const events::ChannelUpdate&)> cb) {
    dispatcher_.on_channel_update(cb);
}

void cluster::on_channel_delete(std::function<void(const events::ChannelDelete&)> cb) {
    dispatcher_.on_channel_delete(cb);
}

void cluster::on_server_create(std::function<void(const events::ServerCreate&)> cb) {
    dispatcher_.on_server_create(cb);
}

void cluster::on_server_update(std::function<void(const events::ServerUpdate&)> cb) {
    dispatcher_.on_server_update(cb);
}

void cluster::on_server_delete(std::function<void(const events::ServerDelete&)> cb) {
    dispatcher_.on_server_delete(cb);
}

void cluster::on_server_member_join(std::function<void(const events::ServerMemberJoin&)> cb) {
    dispatcher_.on_server_member_join(cb);
}

void cluster::on_server_member_leave(std::function<void(const events::ServerMemberLeave&)> cb) {
    dispatcher_.on_server_member_leave(cb);
}

void cluster::on_server_member_update(std::function<void(const events::ServerMemberUpdate&)> cb) {
    dispatcher_.on_server_member_update(cb);
}

void cluster::on_user_update(std::function<void(const events::UserUpdate&)> cb) {
    dispatcher_.on_user_update(cb);
}

void cluster::on_channel_start_typing(std::function<void(const events::ChannelStartTyping&)> cb) {
    dispatcher_.on_channel_start_typing(cb);
}

void cluster::on_channel_stop_typing(std::function<void(const events::ChannelStopTyping&)> cb) {
    dispatcher_.on_channel_stop_typing(cb);
}

void cluster::on_error(std::function<void(const events::Error&)> cb) {
    dispatcher_.on_error(cb);
}

void cluster::on_logout(std::function<void(const events::Logout&)> cb) {
    dispatcher_.on_logout(cb);
}

void cluster::on_raw_event(std::function<void(const std::string& type, const nlohmann::json& data)> cb) {
    dispatcher_.on_raw_event(cb);
}

void cluster::send_message(const std::string& channel_id,
                           const std::string& content,
                           std::function<void(models::Message, bool success)> callback) {
    // Stub
}

void cluster::send_message(const std::string& channel_id,
                           const models::MessagePayload& payload,
                           std::function<void(models::Message, bool)> callback) {
    // Stub
}

void cluster::edit_message(const std::string& channel_id,
                           const std::string& message_id,
                           const std::string& new_content,
                           std::function<void(models::Message, bool)> callback) {
    // Stub
}

void cluster::delete_message(const std::string& channel_id,
                             const std::string& message_id,
                             std::function<void(bool)> callback) {
    // Stub
}

void cluster::react_to_message(const std::string& channel_id,
                               const std::string& message_id,
                               const std::string& emoji_id,
                               std::function<void(bool)> callback) {
    // Stub
}

void cluster::begin_typing(const std::string& channel_id) {
    // Stub
}

void cluster::end_typing(const std::string& channel_id) {
    // Stub
}

rest_client& cluster::rest() {
    return rest_;
}

gateway& cluster::ws() {
    return gateway_;
}

std::optional<models::Server> cluster::get_server(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = server_cache_.find(id);
    if (it != server_cache_.end()) return it->second;
    return std::nullopt;
}

std::optional<models::Channel> cluster::get_channel(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = channel_cache_.find(id);
    if (it != channel_cache_.end()) return it->second;
    return std::nullopt;
}

std::optional<models::User> cluster::get_user(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = user_cache_.find(id);
    if (it != user_cache_.end()) return it->second;
    return std::nullopt;
}

models::User cluster::current_user() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return current_user_;
}

const ClientConfig& cluster::config() const {
    return config_;
}

} // namespace stoatpp
