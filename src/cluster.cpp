#include "stoatpp/cluster.h"
#include "stoatpp/bot_module.h"
#include "stoatpp/utils/logger.h"
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>

namespace stoatpp {

static std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::hex;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << std::setw(2) << std::setfill('0') << (static_cast<int>(c) & 0xFF);
        }
    }
    return escaped.str();
}

cluster::cluster(const std::string& token, ClientConfig config)
    : token_(token),
      config_(config),
      rest_(token, config),
      dispatcher_(),
      gateway_(token, config, dispatcher_) {

    // 1. Automatic prefix-based command routing
    this->on_message([this](const events::Message& msg) {
        if (!msg.author.username.empty()) {
            std::lock_guard<std::shared_mutex> lock(cache_mutex_);
            user_cache_[msg.author.id] = msg.author;
        }
        if (msg.raw.contains("user") && msg.raw["user"].is_object()) {
            auto usr = models::User::from_json(msg.raw["user"]);
            std::lock_guard<std::shared_mutex> lock(cache_mutex_);
            user_cache_[usr.id] = usr;
        }

        const std::string& prefix = config_.command_prefix;
        if (msg.content.rfind(prefix, 0) != 0) return;

        std::stringstream ss(msg.content.substr(prefix.size()));
        std::string cmd;
        ss >> cmd;

        std::vector<std::string> args;
        std::string arg;
        while (ss >> arg) {
            args.push_back(arg);
        }

        auto it = commands_.find(cmd);
        if (it != commands_.end()) {
            it->second(*this, msg, args);
        }
    });

    // 2. Local cache synchronisation via ready and incoming events
    this->on_ready([this](const events::Ready& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG, "Caching Ready data: user=" + e.user.username + ", servers=" + std::to_string(e.servers.size()) + ", channels=" + std::to_string(e.channels.size()), config_);
        current_user_ = e.user;
        user_cache_[e.user.id] = e.user;
        
        for (const auto& s : e.servers) {
            server_cache_[s.id] = s;
        }
        for (const auto& c : e.channels) {
            channel_cache_[c.id] = c;
        }
    });

    this->on_channel_create([this](const events::ChannelCreate& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG, "Caching new channel: " + e.channel.id, config_);
        channel_cache_[e.channel.id] = e.channel;
    });

    this->on_channel_delete([this](const events::ChannelDelete& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG, "Removing channel from cache: " + e.id, config_);
        channel_cache_.erase(e.id);
    });

    this->on_server_create([this](const events::ServerCreate& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG, "Caching new server: " + e.server.name + " (" + e.server.id + ")", config_);
        server_cache_[e.server.id] = e.server;
    });

    this->on_server_delete([this](const events::ServerDelete& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG, "Removing server from cache: " + e.id, config_);
        server_cache_.erase(e.id);
    });
}

void cluster::start(bool return_after_init) {
    utils::logger::log(LogLevel::INFO, "Starting cluster...", config_);
    
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
    gateway_.disconnect();
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

void cluster::send_message(const std::string& channel_id,
                           const std::string& content,
                           std::function<void(models::Message, bool success)> callback) {
    models::MessagePayload payload;
    payload.content = content;
    send_message(channel_id, payload, callback);
}

void cluster::send_message(const std::string& channel_id,
                           const models::MessagePayload& payload,
                           std::function<void(models::Message, bool)> callback) {
    std::thread([this, channel_id, payload, callback]() {
        try {
            std::string path = "/channels/" + channel_id + "/messages";
            auto res = rest_.post(path, payload.to_json());
            if (res.success()) {
                auto msg = models::Message::from_json(res.body);
                if (callback) callback(msg, true);

                if (payload.delete_after > 0) {
                    std::string msg_id = msg.id;
                    int delay = payload.delete_after;
                    std::thread([this, channel_id, msg_id, delay]() {
                        std::this_thread::sleep_for(std::chrono::seconds(delay));
                        rest_.del("/channels/" + channel_id + "/messages/" + msg_id);
                    }).detach();
                }
            } else {
                utils::logger::log(LogLevel::ERROR, "Failed to send message: " + res.error_message(), config_);
                if (callback) callback(models::Message{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in send_message: " + std::string(e.what()), config_);
            if (callback) callback(models::Message{}, false);
        }
    }).detach();
}


void cluster::edit_message(const std::string& channel_id,
                           const std::string& message_id,
                           const std::string& new_content,
                           std::function<void(models::Message, bool)> callback) {
    std::thread([this, channel_id, message_id, new_content, callback]() {
        try {
            std::string path = "/channels/" + channel_id + "/messages/" + message_id;
            nlohmann::json body = {{"content", new_content}};
            auto res = rest_.patch(path, body);
            if (res.success()) {
                auto msg = models::Message::from_json(res.body);
                if (callback) callback(msg, true);
            } else {
                utils::logger::log(LogLevel::ERROR, "Failed to edit message: " + res.error_message(), config_);
                if (callback) callback(models::Message{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in edit_message: " + std::string(e.what()), config_);
            if (callback) callback(models::Message{}, false);
        }
    }).detach();
}

void cluster::edit_message(const std::string& channel_id,
                           const std::string& message_id,
                           const models::MessagePayload& payload,
                           std::function<void(models::Message, bool)> callback) {
    std::thread([this, channel_id, message_id, payload, callback]() {
        try {
            std::string path = "/channels/" + channel_id + "/messages/" + message_id;
            auto res = rest_.patch(path, payload.to_json());
            if (res.success()) {
                auto msg = models::Message::from_json(res.body);
                if (callback) callback(msg, true);
            } else {
                utils::logger::log(LogLevel::ERROR, "Failed to edit message: " + res.error_message(), config_);
                if (callback) callback(models::Message{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in edit_message: " + std::string(e.what()), config_);
            if (callback) callback(models::Message{}, false);
        }
    }).detach();
}

void cluster::delete_message(const std::string& channel_id,
                             const std::string& message_id,
                             std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, callback]() {
        try {
            std::string path = "/channels/" + channel_id + "/messages/" + message_id;
            auto res = rest_.del(path);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in delete_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::react_to_message(const std::string& channel_id,
                               const std::string& message_id,
                               const std::string& emoji_id,
                               std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, emoji_id, callback]() {
        try {
            auto res = rest_.add_reaction(channel_id, message_id, emoji_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in react_to_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::unreact_from_message(const std::string& channel_id,
                                   const std::string& message_id,
                                   const std::string& emoji_id,
                                   const std::optional<std::string>& user_id,
                                   std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, emoji_id, user_id, callback]() {
        try {
            auto res = rest_.remove_reaction(channel_id, message_id, emoji_id, user_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in unreact_from_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::begin_typing(const std::string& channel_id) {
    gateway_.begin_typing(channel_id);
}

void cluster::end_typing(const std::string& channel_id) {
    gateway_.end_typing(channel_id);
}

void cluster::ban_user(const std::string& server_id,
                      const std::string& user_id,
                      const std::string& reason,
                      std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, reason, callback]() {
        try {
            auto res = rest_.ban_user(server_id, user_id, reason.empty() ? std::optional<std::string>{} : reason);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in ban_user: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::unban_user(const std::string& server_id,
                        const std::string& user_id,
                        std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, callback]() {
        try {
            auto res = rest_.unban_user(server_id, user_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in unban_user: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::kick_member(const std::string& server_id,
                         const std::string& user_id,
                         std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, callback]() {
        try {
            auto res = rest_.kick_member(server_id, user_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in kick_member: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::timeout_member(const std::string& server_id,
                            const std::string& user_id,
                            const std::string& duration_iso,
                            std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, duration_iso, callback]() {
        try {
            nlohmann::json fields;
            fields["timeout"] = duration_iso;
            auto res = rest_.edit_member(server_id, user_id, fields);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in timeout_member: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::remove_timeout(const std::string& server_id,
                            const std::string& user_id,
                            std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, callback]() {
        try {
            nlohmann::json fields;
            fields["timeout"] = nullptr;
            auto res = rest_.edit_member(server_id, user_id, fields);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in remove_timeout: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::create_role(const std::string& server_id,
                         const std::string& name,
                         std::function<void(models::Role, bool)> callback) {
    std::thread([this, server_id, name, callback]() {
        try {
            auto res = rest_.create_role(server_id, name);
            if (res.success()) {
                auto r = models::Role::from_json(res.body);
                if (callback) callback(r, true);
            } else {
                if (callback) callback(models::Role{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in create_role: " + std::string(e.what()), config_);
            if (callback) callback(models::Role{}, false);
        }
    }).detach();
}

void cluster::delete_role(const std::string& server_id,
                         const std::string& role_id,
                         std::function<void(bool)> callback) {
    std::thread([this, server_id, role_id, callback]() {
        try {
            auto res = rest_.delete_role(server_id, role_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in delete_role: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::add_role_to_member(const std::string& server_id,
                                 const std::string& user_id,
                                 const std::string& role_id,
                                 std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, role_id, callback]() {
        try {
            auto get_res = rest_.get("/servers/" + server_id + "/members/" + user_id);
            if (!get_res.success()) {
                if (callback) callback(false);
                return;
            }
            auto mem = models::Member::from_json(get_res.body);
            bool has_role = false;
            for (const auto& r : mem.roles) {
                if (r == role_id) {
                    has_role = true;
                    break;
                }
            }
            if (has_role) {
                if (callback) callback(true);
                return;
            }
            mem.roles.push_back(role_id);
            nlohmann::json fields;
            fields["roles"] = mem.roles;
            auto patch_res = rest_.edit_member(server_id, user_id, fields);
            if (callback) callback(patch_res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in add_role_to_member: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::remove_role_from_member(const std::string& server_id,
                                      const std::string& user_id,
                                      const std::string& role_id,
                                      std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, role_id, callback]() {
        try {
            auto get_res = rest_.get("/servers/" + server_id + "/members/" + user_id);
            if (!get_res.success()) {
                if (callback) callback(false);
                return;
            }
            auto mem = models::Member::from_json(get_res.body);
            std::vector<std::string> new_roles;
            bool found = false;
            for (const auto& r : mem.roles) {
                if (r == role_id) {
                    found = true;
                } else {
                    new_roles.push_back(r);
                }
            }
            if (!found) {
                if (callback) callback(true);
                return;
            }
            nlohmann::json fields;
            fields["roles"] = new_roles;
            auto patch_res = rest_.edit_member(server_id, user_id, fields);
            if (callback) callback(patch_res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in remove_role_from_member: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::get_member_count(const std::string& server_id,
                               std::function<void(int, bool)> callback) {
    std::thread([this, server_id, callback]() {
        try {
            auto res = rest_.get_server_members(server_id);
            if (res.success()) {
                if (res.body.is_array()) {
                    if (callback) callback(static_cast<int>(res.body.size()), true);
                } else if (res.body.is_object() && res.body.contains("members") && res.body["members"].is_array()) {
                    if (callback) callback(static_cast<int>(res.body["members"].size()), true);
                } else {
                    if (callback) callback(0, false);
                }
            } else {
                if (callback) callback(0, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in get_member_count: " + std::string(e.what()), config_);
            if (callback) callback(0, false);
        }
    }).detach();
}

void cluster::fetch_server_invites(const std::string& server_id,
                                   std::function<void(std::vector<models::Invite>, bool)> callback) {
    std::thread([this, server_id, callback]() {
        try {
            auto res = rest_.get_server_invites(server_id);
            if (res.success() && res.body.is_array()) {
                std::vector<models::Invite> invites;
                for (const auto& j : res.body) {
                    invites.push_back(models::Invite::from_json(j));
                }
                if (callback) callback(invites, true);
            } else {
                if (callback) callback({}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in fetch_server_invites: " + std::string(e.what()), config_);
            if (callback) callback({}, false);
        }
    }).detach();
}

void cluster::create_invite(const std::string& channel_id,
                            std::function<void(models::Invite, bool)> callback) {
    std::thread([this, channel_id, callback]() {
        try {
            auto res = rest_.create_channel_invite(channel_id);
            if (res.success()) {
                auto inv = models::Invite::from_json(res.body);
                if (callback) callback(inv, true);
            } else {
                if (callback) callback(models::Invite{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in create_invite: " + std::string(e.what()), config_);
            if (callback) callback(models::Invite{}, false);
        }
    }).detach();
}

void cluster::set_slowmode(const std::string& channel_id,
                          int seconds,
                          std::function<void(bool)> callback) {
    std::thread([this, channel_id, seconds, callback]() {
        try {
            nlohmann::json fields;
            fields["slowmode"] = seconds;
            auto res = rest_.edit_channel(channel_id, fields);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in set_slowmode: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::clear_slowmode(const std::string& channel_id,
                            std::function<void(bool)> callback) {
    std::thread([this, channel_id, callback]() {
        try {
            nlohmann::json fields;
            fields["slowmode"] = 0;
            auto res = rest_.edit_channel(channel_id, fields);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in clear_slowmode: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::update_status(const std::string& text,
                           const std::string& presence,
                           std::function<void(bool)> callback) {
    std::thread([this, text, presence, callback]() {
        try {
            nlohmann::json status_obj;
            status_obj["text"] = text;
            status_obj["presence"] = presence;
            
            nlohmann::json body;
            body["status"] = status_obj;
            
            auto res = rest_.patch("/users/@me", body);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in update_status: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::fetch_channel_permissions(const std::string& channel_id,
                                        std::function<void(nlohmann::json, bool)> callback) {
    std::thread([this, channel_id, callback]() {
        try {
            auto res = rest_.get_channel_permissions(channel_id);
            if (res.success()) {
                if (callback) callback(res.body, true);
            } else {
                if (callback) callback(nlohmann::json::object(), false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in fetch_channel_permissions: " + std::string(e.what()), config_);
            if (callback) callback(nlohmann::json::object(), false);
        }
    }).detach();
}

void cluster::set_channel_permissions(const std::string& channel_id,
                                      const std::string& role_id,
                                      int64_t allow_mask,
                                      int64_t deny_mask,
                                      std::function<void(bool)> callback) {
    std::thread([this, channel_id, role_id, allow_mask, deny_mask, callback]() {
        try {
            auto res = rest_.set_channel_permission(channel_id, role_id, allow_mask, deny_mask);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in set_channel_permissions: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
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

int64_t cluster::ping_latency() const {
    return gateway_.ping_latency();
}

void cluster::use(std::unique_ptr<bot_module> module) {
    if (module) {
        module->register_handlers(*this);
        modules_.push_back(std::move(module));
    }
}

void cluster::register_command(const std::string& name,
                              std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb) {
    commands_[name] = cb;
}

void cluster::register_command(const std::string& name,
                              const std::vector<std::string>& aliases,
                              std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb) {
    register_command(name, cb);
    for (const auto& alias : aliases) {
        register_command(alias, cb);
    }
}

void cluster::fetch_user(const std::string& user_id, std::function<void(models::User, bool success)> callback) {
    auto cached = get_user(user_id);
    if (cached && cached->bio.has_value()) {
        if (callback) callback(*cached, true);
        return;
    }

    std::thread([this, user_id, cached, callback]() {
        try {
            models::User usr;
            bool got_user = false;
            if (cached) {
                usr = *cached;
                got_user = true;
            }

            if (!got_user) {
                auto res = rest_.get("/users/" + user_id);
                if (res.success()) {
                    usr = models::User::from_json(res.body);
                    got_user = true;
                }
            }

            if (!got_user) {
                if (callback) callback(models::User{}, false);
                return;
            }

            auto prof_res = rest_.get("/users/" + user_id + "/profile");
            if (prof_res.success()) {
                const auto& prof = prof_res.body;
                if (prof.contains("content") && prof["content"].is_string()) {
                    usr.bio = prof["content"].get<std::string>();
                }
                if (prof.contains("background") && !prof["background"].is_null()) {
                    if (prof["background"].is_string()) {
                        usr.banner = prof["background"].get<std::string>();
                    } else if (prof["background"].is_object()) {
                        if (prof["background"].contains("id") && prof["background"]["id"].is_string()) {
                            usr.banner = prof["background"]["id"].get<std::string>();
                        } else if (prof["background"].contains("_id") && prof["background"]["_id"].is_string()) {
                            usr.banner = prof["background"]["_id"].get<std::string>();
                        }
                    }
                }
            } else {
                usr.bio = "";
                usr.banner = "";
            }

            {
                std::lock_guard<std::shared_mutex> lock(cache_mutex_);
                user_cache_[usr.id] = usr;
            }
            if (callback) callback(usr, true);
        } catch (...) {
            if (callback) callback(models::User{}, false);
        }
    }).detach();
}

void cluster::fetch_member(const std::string& server_id, const std::string& user_id, std::function<void(models::Member, bool success)> callback) {
    std::thread([this, server_id, user_id, callback]() {
        try {
            auto res = rest_.get("/servers/" + server_id + "/members/" + user_id);
            if (res.success()) {
                auto mem = models::Member::from_json(res.body);
                if (callback) callback(mem, true);
            } else {
                if (callback) callback(models::Member{}, false);
            }
        } catch (...) {
            if (callback) callback(models::Member{}, false);
        }
    }).detach();
}

} // namespace stoatpp
