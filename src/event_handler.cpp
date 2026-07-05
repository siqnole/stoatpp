#include "stoatpp/event_handler.h"

namespace stoatpp {

void event_dispatcher::on_ready(std::function<void(const events::Ready&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    ready_handlers_.push_back(cb);
}

void event_dispatcher::on_message(std::function<void(const events::Message&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_handlers_.push_back(cb);
}

void event_dispatcher::on_message_update(std::function<void(const events::MessageUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_update_handlers_.push_back(cb);
}

void event_dispatcher::on_message_delete(std::function<void(const events::MessageDelete&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_delete_handlers_.push_back(cb);
}

void event_dispatcher::on_message_react(std::function<void(const events::MessageReact&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_react_handlers_.push_back(cb);
}

void event_dispatcher::on_message_unreact(std::function<void(const events::MessageUnreact&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_unreact_handlers_.push_back(cb);
}

void event_dispatcher::on_channel_create(std::function<void(const events::ChannelCreate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel_create_handlers_.push_back(cb);
}

void event_dispatcher::on_channel_update(std::function<void(const events::ChannelUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel_update_handlers_.push_back(cb);
}

void event_dispatcher::on_channel_delete(std::function<void(const events::ChannelDelete&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel_delete_handlers_.push_back(cb);
}

void event_dispatcher::on_server_create(std::function<void(const events::ServerCreate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_create_handlers_.push_back(cb);
}

void event_dispatcher::on_server_update(std::function<void(const events::ServerUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_update_handlers_.push_back(cb);
}

void event_dispatcher::on_server_delete(std::function<void(const events::ServerDelete&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_delete_handlers_.push_back(cb);
}

void event_dispatcher::on_server_member_join(std::function<void(const events::ServerMemberJoin&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_member_join_handlers_.push_back(cb);
}

void event_dispatcher::on_server_member_leave(std::function<void(const events::ServerMemberLeave&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_member_leave_handlers_.push_back(cb);
}

void event_dispatcher::on_server_member_update(std::function<void(const events::ServerMemberUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_member_update_handlers_.push_back(cb);
}

void event_dispatcher::on_user_update(std::function<void(const events::UserUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_update_handlers_.push_back(cb);
}

void event_dispatcher::on_server_role_create(std::function<void(const events::ServerRoleCreate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_role_create_handlers_.push_back(cb);
}

void event_dispatcher::on_server_role_update(std::function<void(const events::ServerRoleUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_role_update_handlers_.push_back(cb);
}

void event_dispatcher::on_server_role_delete(std::function<void(const events::ServerRoleDelete&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_role_delete_handlers_.push_back(cb);
}

void event_dispatcher::on_user_settings_update(std::function<void(const events::UserSettingsUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_settings_update_handlers_.push_back(cb);
}

void event_dispatcher::on_user_presence_update(std::function<void(const events::UserPresenceUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_presence_update_handlers_.push_back(cb);
}

void event_dispatcher::on_webhook_update(std::function<void(const events::WebhookUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    webhook_update_handlers_.push_back(cb);
}

void event_dispatcher::on_voice_state_update(std::function<void(const events::VoiceStateUpdate&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    voice_state_update_handlers_.push_back(cb);
}

void event_dispatcher::on_channel_start_typing(std::function<void(const events::ChannelStartTyping&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel_start_typing_handlers_.push_back(cb);
}

void event_dispatcher::on_channel_stop_typing(std::function<void(const events::ChannelStopTyping&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel_stop_typing_handlers_.push_back(cb);
}

void event_dispatcher::on_error(std::function<void(const events::Error&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_handlers_.push_back(cb);
}

void event_dispatcher::on_logout(std::function<void(const events::Logout&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    logout_handlers_.push_back(cb);
}

void event_dispatcher::on_raw_event(std::function<void(const std::string&, const nlohmann::json&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    raw_event_handlers_.push_back(cb);
}

// Dispatches
void event_dispatcher::dispatch_ready(const events::Ready& e) {
    std::vector<std::function<void(const events::Ready&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = ready_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_message(const events::Message& e) {
    std::vector<std::function<void(const events::Message&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = message_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_message_update(const events::MessageUpdate& e) {
    std::vector<std::function<void(const events::MessageUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = message_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_message_delete(const events::MessageDelete& e) {
    std::vector<std::function<void(const events::MessageDelete&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = message_delete_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_message_react(const events::MessageReact& e) {
    std::vector<std::function<void(const events::MessageReact&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = message_react_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_message_unreact(const events::MessageUnreact& e) {
    std::vector<std::function<void(const events::MessageUnreact&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = message_unreact_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_channel_create(const events::ChannelCreate& e) {
    std::vector<std::function<void(const events::ChannelCreate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = channel_create_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_channel_update(const events::ChannelUpdate& e) {
    std::vector<std::function<void(const events::ChannelUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = channel_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_channel_delete(const events::ChannelDelete& e) {
    std::vector<std::function<void(const events::ChannelDelete&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = channel_delete_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_create(const events::ServerCreate& e) {
    std::vector<std::function<void(const events::ServerCreate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_create_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_update(const events::ServerUpdate& e) {
    std::vector<std::function<void(const events::ServerUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_delete(const events::ServerDelete& e) {
    std::vector<std::function<void(const events::ServerDelete&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_delete_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_member_join(const events::ServerMemberJoin& e) {
    std::vector<std::function<void(const events::ServerMemberJoin&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_member_join_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_member_leave(const events::ServerMemberLeave& e) {
    std::vector<std::function<void(const events::ServerMemberLeave&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_member_leave_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_member_update(const events::ServerMemberUpdate& e) {
    std::vector<std::function<void(const events::ServerMemberUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_member_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_user_update(const events::UserUpdate& e) {
    std::vector<std::function<void(const events::UserUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = user_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_role_create(const events::ServerRoleCreate& e) {
    std::vector<std::function<void(const events::ServerRoleCreate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_role_create_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_role_update(const events::ServerRoleUpdate& e) {
    std::vector<std::function<void(const events::ServerRoleUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_role_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_server_role_delete(const events::ServerRoleDelete& e) {
    std::vector<std::function<void(const events::ServerRoleDelete&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = server_role_delete_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_user_settings_update(const events::UserSettingsUpdate& e) {
    std::vector<std::function<void(const events::UserSettingsUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = user_settings_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_user_presence_update(const events::UserPresenceUpdate& e) {
    std::vector<std::function<void(const events::UserPresenceUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = user_presence_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_webhook_update(const events::WebhookUpdate& e) {
    std::vector<std::function<void(const events::WebhookUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = webhook_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_voice_state_update(const events::VoiceStateUpdate& e) {
    std::vector<std::function<void(const events::VoiceStateUpdate&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = voice_state_update_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_channel_start_typing(const events::ChannelStartTyping& e) {
    std::vector<std::function<void(const events::ChannelStartTyping&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = channel_start_typing_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_channel_stop_typing(const events::ChannelStopTyping& e) {
    std::vector<std::function<void(const events::ChannelStopTyping&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = channel_stop_typing_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_error(const events::Error& e) {
    std::vector<std::function<void(const events::Error&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = error_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_logout(const events::Logout& e) {
    std::vector<std::function<void(const events::Logout&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = logout_handlers_;
    }
    for (auto& h : handlers) h(e);
}

void event_dispatcher::dispatch_raw_event(const std::string& type, const nlohmann::json& data) {
    std::vector<std::function<void(const std::string&, const nlohmann::json&)>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = raw_event_handlers_;
    }
    for (auto& h : handlers) h(type, data);
}

events::Message events::Message::from_json(const nlohmann::json& j) {
    events::Message ev;
    if (j.contains("id")) ev.id = j["id"].get<std::string>();
    else if (j.contains("_id")) ev.id = j["_id"].get<std::string>();
    
    if (j.contains("channel")) ev.channel_id = j["channel"].get<std::string>();
    else if (j.contains("channel_id")) ev.channel_id = j["channel_id"].get<std::string>();

    if (j.contains("server")) {
        ev.server_id = j["server"].get<std::string>();
    } else if (j.contains("member") && j["member"].is_object() && j["member"].contains("_id") && j["member"]["_id"].is_object() && j["member"]["_id"].contains("server")) {
        ev.server_id = j["member"]["_id"]["server"].get<std::string>();
    }
    
    if (j.contains("user") && j["user"].is_object()) {
        ev.author = models::User::from_json(j["user"]);
    } else if (j.contains("author")) {
        if (j["author"].is_object()) {
            ev.author = models::User::from_json(j["author"]);
        } else if (j["author"].is_string()) {
            ev.author.id = j["author"].get<std::string>();
        }
    }
    
    if (j.contains("webhook") && j["webhook"].is_object() && j["webhook"].contains("name")) {
        ev.author.username = j["webhook"]["name"].get<std::string>();
    }

    if (j.contains("content")) ev.content = j["content"].get<std::string>();
    if (j.contains("nonce")) ev.nonce = j["nonce"].get<std::string>();
    if (j.contains("edited") && j["edited"].is_boolean()) ev.edited = j["edited"].get<bool>();

    if (j.contains("replies") && j["replies"].is_array()) {
        for (const auto& r : j["replies"]) {
            if (r.is_string()) ev.replies.push_back(r.get<std::string>());
        }
    }
    if (j.contains("mentions") && j["mentions"].is_array()) {
        for (const auto& m : j["mentions"]) {
            if (m.is_string()) ev.mentions.push_back(m.get<std::string>());
        }
    }
    if (j.contains("attachments") && j["attachments"].is_array()) {
        for (const auto& a : j["attachments"]) {
            if (a.is_string()) ev.attachments.push_back(a.get<std::string>());
            else if (a.is_object() && a.contains("id") && a["id"].is_string())
                ev.attachments.push_back(a["id"].get<std::string>());
            else if (a.is_object() && a.contains("_id") && a["_id"].is_string())
                ev.attachments.push_back(a["_id"].get<std::string>());
        }
    }

    ev.raw = j;
    return ev;
}

} // namespace stoatpp
