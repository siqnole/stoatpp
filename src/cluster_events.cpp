#include "stoatpp/cluster.h"

namespace stoatpp {

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

void cluster::on_server_role_create(std::function<void(const events::ServerRoleCreate&)> cb) {
    dispatcher_.on_server_role_create(cb);
}

void cluster::on_server_role_update(std::function<void(const events::ServerRoleUpdate&)> cb) {
    dispatcher_.on_server_role_update(cb);
}

void cluster::on_server_role_delete(std::function<void(const events::ServerRoleDelete&)> cb) {
    dispatcher_.on_server_role_delete(cb);
}

void cluster::on_user_update(std::function<void(const events::UserUpdate&)> cb) {
    dispatcher_.on_user_update(cb);
}

void cluster::on_user_settings_update(std::function<void(const events::UserSettingsUpdate&)> cb) {
    dispatcher_.on_user_settings_update(cb);
}

void cluster::on_user_presence_update(std::function<void(const events::UserPresenceUpdate&)> cb) {
    dispatcher_.on_user_presence_update(cb);
}

void cluster::on_channel_start_typing(std::function<void(const events::ChannelStartTyping&)> cb) {
    dispatcher_.on_channel_start_typing(cb);
}

void cluster::on_channel_stop_typing(std::function<void(const events::ChannelStopTyping&)> cb) {
    dispatcher_.on_channel_stop_typing(cb);
}

void cluster::on_webhook_update(std::function<void(const events::WebhookUpdate&)> cb) {
    dispatcher_.on_webhook_update(cb);
}

void cluster::on_voice_state_update(std::function<void(const events::VoiceStateUpdate&)> cb) {
    dispatcher_.on_voice_state_update(cb);
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

} // namespace stoatpp
