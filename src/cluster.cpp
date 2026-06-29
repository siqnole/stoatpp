#include "stoatpp/cluster.h"
#include "stoatpp/bot_module.h"
#include "stoatpp/utils/logger.h"
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <algorithm>

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
      gateway_(token, config, dispatcher_),
      launch_time_(std::chrono::steady_clock::now()) {

    rest_.set_error_callback([this](const std::string& method, const std::string& path, int status_code, const std::string& error_msg) {
        if (rest_error_handler_) {
            rest_error_handler_(method, path, status_code, error_msg);
        }
    });

    if (config_.enable_default_help) {
        register_command("help", [this](cluster& bot, const events::Message& msg, const std::vector<std::string>& args) {
            std::vector<Command> cmds = registered_commands_;
            std::sort(cmds.begin(), cmds.end(), [](const Command& a, const Command& b) {
                return a.name < b.name;
            });

            std::string prefix = config_.command_prefix;
            if (config_.prefix_resolver) {
                auto resolved = config_.prefix_resolver(bot, msg);
                if (!resolved.empty()) {
                    prefix = resolved[0];
                }
            }

            int max_pages = (cmds.size() + 2) / 3;
            if (max_pages == 0) max_pages = 1;

            stoatpp::models::MessagePayload payload;
            nlohmann::json embed = nlohmann::json::object();
            embed["colour"] = config_.default_help_color;
            embed["title"] = "stoat++ default help menu (page 1 of " + std::to_string(max_pages) + ")";

            std::string desc = "here is a list of all registered commands:\n\n";
            for (size_t i = 0; i < 3 && i < cmds.size(); ++i) {
                const auto& cmd = cmds[i];
                desc += "• **" + prefix + cmd.name + "**";
                if (!cmd.aliases.empty()) {
                    desc += " (aliases: ";
                    for (size_t a = 0; a < cmd.aliases.size(); ++a) {
                        desc += "`" + prefix + cmd.aliases[a] + "`" + (a + 1 < cmd.aliases.size() ? ", " : "");
                    }
                    desc += ")";
                }
                desc += "\n";

                std::string syntax = prefix + cmd.name;
                if (!cmd.args.empty()) {
                    for (const auto& arg : cmd.args) {
                        syntax += " " + arg;
                    }
                } else if (!cmd.usage.empty()) {
                    syntax = cmd.usage;
                }
                desc += "  *syntax:* `" + syntax + "`\n";

                if (!cmd.description.empty()) {
                    desc += "  *description:* " + cmd.description + "\n";
                }
                desc += "\n";
            }
            embed["description"] = desc;
            payload.embeds.push_back(embed);

            bot.send_message(msg.channel_id, payload, [this, msg, max_pages](stoatpp::models::Message res, bool success) {
                if (success && max_pages > 1) {
                    {
                        std::lock_guard<std::mutex> lock(help_sessions_mutex_);
                        help_sessions_[res.id] = {0, max_pages, msg.author.id};
                    }
                    react_to_message(msg.channel_id, res.id, "⬅");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    react_to_message(msg.channel_id, res.id, "➡");
                }
            });
        });

        this->on_message_react([this](const events::MessageReact& e) {
            std::unique_lock<std::mutex> lock(help_sessions_mutex_);
            auto it = help_sessions_.find(e.id);
            if (it != help_sessions_.end()) {
                auto& session = it->second;
                if (e.user_id == current_user().id) return;
                if (e.user_id != session.user_id) {
                    lock.unlock();
                    unreact_from_message(e.channel_id, e.id, e.emoji_id, e.user_id);
                    return;
                }

                bool page_changed = false;
                if (e.emoji_id == "⬅️" || e.emoji_id == "◀" || e.emoji_id == "⬅") {
                    if (session.current_page > 0) {
                        session.current_page--;
                    } else {
                        session.current_page = session.max_pages - 1;
                    }
                    page_changed = true;
                } else if (e.emoji_id == "➡️" || e.emoji_id == "▶" || e.emoji_id == "➡") {
                    if (session.current_page < session.max_pages - 1) {
                        session.current_page++;
                    } else {
                        session.current_page = 0;
                    }
                    page_changed = true;
                }

                if (page_changed) {
                    int page = session.current_page;
                    int max_pages = session.max_pages;
                    lock.unlock();

                    events::Message dummy_msg;
                    dummy_msg.author.id = e.user_id;
                    dummy_msg.channel_id = e.channel_id;
                    {
                        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);
                        auto ch_it = channel_cache_.find(e.channel_id);
                        if (ch_it != channel_cache_.end()) {
                            dummy_msg.server_id = ch_it->second.server.value_or("");
                        }
                    }

                    std::string prefix = config_.command_prefix;
                    if (config_.prefix_resolver) {
                        auto resolved = config_.prefix_resolver(*this, dummy_msg);
                        if (!resolved.empty()) {
                            prefix = resolved[0];
                        }
                    }

                    std::vector<Command> cmds = registered_commands_;
                    std::sort(cmds.begin(), cmds.end(), [](const Command& a, const Command& b) {
                        return a.name < b.name;
                    });

                    stoatpp::models::MessagePayload payload;
                    payload.content = " ";
                    nlohmann::json embed = nlohmann::json::object();
                    embed["colour"] = config_.default_help_color;
                    embed["title"] = "stoat++ default help menu (page " + std::to_string(page + 1) + " of " + std::to_string(max_pages) + ")";

                    std::string desc = "here is a list of all registered commands:\n\n";
                    size_t start = page * 3;
                    for (size_t i = start; i < start + 3 && i < cmds.size(); ++i) {
                        const auto& cmd = cmds[i];
                        desc += "• **" + prefix + cmd.name + "**";
                        if (!cmd.aliases.empty()) {
                            desc += " (aliases: ";
                            for (size_t a = 0; a < cmd.aliases.size(); ++a) {
                                desc += "`" + prefix + cmd.aliases[a] + "`" + (a + 1 < cmd.aliases.size() ? ", " : "");
                            }
                            desc += ")";
                        }
                        desc += "\n";

                        std::string syntax = prefix + cmd.name;
                        if (!cmd.args.empty()) {
                            for (const auto& arg : cmd.args) {
                                syntax += " " + arg;
                            }
                        } else if (!cmd.usage.empty()) {
                            syntax = cmd.usage;
                        }
                        desc += "  *syntax:* `" + syntax + "`\n";

                        if (!cmd.description.empty()) {
                            desc += "  *description:* " + cmd.description + "\n";
                        }
                        desc += "\n";
                    }
                    embed["description"] = desc;
                    payload.embeds.push_back(embed);

                    edit_message(e.channel_id, e.id, payload, [this, e](auto, bool) {
                        unreact_from_message(e.channel_id, e.id, e.emoji_id, e.user_id);
                    });
                }
            }
        });


    }

    // 1. Automatic prefix-based command routing
    this->on_message([this](const events::Message& msg) {
        std::function<void(std::optional<events::Message>)> cb = nullptr;
        {
            std::lock_guard<std::mutex> lock(message_watchers_mutex_);
            auto it = std::find_if(message_watchers_.begin(), message_watchers_.end(),
                                   [&msg](const MessageWatcher& w) {
                                       if (w.channel_id != msg.channel_id) return false;
                                       if (!w.user_id.empty() && w.user_id != msg.author.id) return false;
                                       return true;
                                   });
            if (it != message_watchers_.end()) {
                cb = it->callback;
                message_watchers_.erase(it);
            }
        }

        if (cb) {
            cb(msg);
            return;
        }

        if (!msg.author.username.empty()) {
            std::lock_guard<std::shared_mutex> lock(cache_mutex_);
            user_cache_[msg.author.id] = msg.author;
        }
        if (msg.raw.contains("user") && msg.raw["user"].is_object()) {
            auto usr = models::User::from_json(msg.raw["user"]);
            std::lock_guard<std::shared_mutex> lock(cache_mutex_);
            user_cache_[usr.id] = usr;
        }

        std::vector<std::string> prefixes;
        if (config_.prefix_resolver) {
            prefixes = config_.prefix_resolver(*this, msg);
        } else {
            prefixes = {config_.command_prefix};
        }

        std::sort(prefixes.begin(), prefixes.end(), [](const std::string& a, const std::string& b) {
            return a.size() > b.size();
        });

        std::string matched_prefix = "";
        for (const auto& p : prefixes) {
            if (!p.empty() && msg.content.rfind(p, 0) == 0) {
                matched_prefix = p;
                break;
            }
        }

        if (matched_prefix.empty()) return;

        std::stringstream ss(msg.content.substr(matched_prefix.size()));
        std::string cmd;
        ss >> cmd;

        std::vector<std::string> args;
        std::string arg;
        while (ss >> arg) {
            args.push_back(arg);
        }

        Command cmd_obj;
        bool found_cmd = false;
        for (const auto& c : registered_commands_) {
            if (c.name == cmd) {
                cmd_obj = c;
                found_cmd = true;
                break;
            }
            for (const auto& a : c.aliases) {
                if (a == cmd) {
                    cmd_obj = c;
                    found_cmd = true;
                    break;
                }
            }
        }

        if (found_cmd) {
            events::Message dispatched_msg = msg;
            dispatched_msg.prefix = matched_prefix;
            
            if (cmd_obj.required_permissions > 0) {
                if (dispatched_msg.server_id.empty()) {
                    send_message(dispatched_msg.channel_id, "this command can only be used in a server.");
                    return;
                }
                
                fetch_member(dispatched_msg.server_id, dispatched_msg.author.id, [this, dispatched_msg, args, cmd_obj](models::Member mem, bool success) {
                    if (!success) {
                        send_message(dispatched_msg.channel_id, "failed to verify your server member permissions.");
                        return;
                    }
                    auto server = get_server(dispatched_msg.server_id);
                    if (!server) {
                        send_message(dispatched_msg.channel_id, "failed to verify server configuration.");
                        return;
                    }
                    if (!has_permission(*server, mem, cmd_obj.required_permissions)) {
                        send_message(dispatched_msg.channel_id, "permission error: you do not have the required permissions to use this command.");
                        return;
                    }
                    cmd_obj.callback(*this, dispatched_msg, args);
                });
            } else {
                cmd_obj.callback(*this, dispatched_msg, args);
            }
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
        utils::logger::log(LogLevel::WARNING, "Could not fetch node info; using default Autumn URL", config_);
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
        timer_thread_.join();
    }

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

void cluster::send_reply(const std::string& channel_id,
                         const std::string& message_id,
                         const std::string& content,
                         bool mention,
                         std::function<void(models::Message, bool success)> callback) {
    models::MessagePayload payload;
    payload.content = content;
    payload.replies.push_back({message_id, mention});
    send_message(channel_id, payload, callback);
}

void cluster::send_reply(const events::Message& to_message,
                         const std::string& content,
                         bool mention,
                         std::function<void(models::Message, bool success)> callback) {
    send_reply(to_message.channel_id, to_message.id, content, mention, callback);
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
                utils::logger::log(LogLevel::ERROR, "Failed to send message: " + res.error_message() + " | body: " + res.body.dump(), config_);
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

void cluster::pin_message(const std::string& channel_id,
                          const std::string& message_id,
                          std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, callback]() {
        try {
            auto res = rest_.pin_message(channel_id, message_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in pin_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::unpin_message(const std::string& channel_id,
                            const std::string& message_id,
                            std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, callback]() {
        try {
            auto res = rest_.unpin_message(channel_id, message_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR, "Exception in unpin_message: " + std::string(e.what()), config_);
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

void cluster::register_command(const Command& cmd) {
    registered_commands_.push_back(cmd);
    commands_[cmd.name] = cmd.callback;
    for (const auto& alias : cmd.aliases) {
        commands_[alias] = cmd.callback;
    }
}

void cluster::register_command(const std::string& name,
                              std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb) {
    Command cmd;
    cmd.name = name;
    cmd.callback = cb;
    register_command(cmd);
}

void cluster::register_command(const std::string& name,
                              const std::vector<std::string>& aliases,
                              std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb) {
    Command cmd;
    cmd.name = name;
    cmd.aliases = aliases;
    cmd.callback = cb;
    register_command(cmd);
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
void cluster::await_message(const std::string& channel_id,
                           const std::string& user_id,
                           std::chrono::milliseconds timeout,
                           std::function<void(std::optional<events::Message>)> callback) {
    static std::atomic<uint64_t> watcher_counter{0};
    std::string watcher_id = "watcher_" + std::to_string(watcher_counter.fetch_add(1));

    {
        std::lock_guard<std::mutex> lock(message_watchers_mutex_);
        message_watchers_.push_back({watcher_id, channel_id, user_id, callback});
    }

    std::thread([this, watcher_id, timeout, callback]() {
        std::this_thread::sleep_for(timeout);
        
        bool triggered = false;
        {
            std::lock_guard<std::mutex> lock(message_watchers_mutex_);
            auto it = std::find_if(message_watchers_.begin(), message_watchers_.end(),
                                   [&watcher_id](const MessageWatcher& w) {
                                       return w.watcher_id == watcher_id;
                                   });
            if (it != message_watchers_.end()) {
                message_watchers_.erase(it);
                triggered = true;
            }
        }

        if (triggered && callback) {
            callback(std::nullopt);
        }
    }).detach();
}

std::chrono::steady_clock::time_point cluster::launch_time() const {
    return launch_time_;
}

uint64_t cluster::uptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - launch_time_).count();
}

const std::string& cluster::token() const {
    return token_;
}

const std::vector<Command>& cluster::get_commands() const {
    return registered_commands_;
}

uint64_t cluster::start_timer(timer_callback cb, uint64_t interval_seconds) {
    uint64_t id = next_timer_id_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(timers_mutex_);
        timers_.push_back({id, cb, interval_seconds, std::chrono::steady_clock::now()});
    }
    timer_cv_.notify_all();
    return id;
}

bool cluster::stop_timer(uint64_t id) {
    std::lock_guard<std::mutex> lock(timers_mutex_);
    auto it = std::find_if(timers_.begin(), timers_.end(), [id](const TimerInfo& t) {
        return t.id == id;
    });
    if (it != timers_.end()) {
        timers_.erase(it);
        return true;
    }
    return false;
}

void cluster::run_timer_loop() {
    while (timers_running_) {
        std::vector<std::pair<timer_callback, timer>> to_run;
        {
            std::lock_guard<std::mutex> lock(timers_mutex_);
            auto now = std::chrono::steady_clock::now();

            for (auto& t : timers_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - t.last_run).count();
                if (elapsed >= static_cast<int64_t>(t.interval_seconds)) {
                    to_run.push_back({t.callback, {t.id, t.interval_seconds}});
                    t.last_run = now;
                }
            }
        }

        // Run callbacks outside the lock
        for (const auto& run : to_run) {
            if (run.first) {
                try {
                    run.first(run.second);
                } catch (const std::exception& e) {
                    utils::logger::log(LogLevel::ERROR, "Exception in timer callback: " + std::string(e.what()), config_);
                } catch (...) {
                    utils::logger::log(LogLevel::ERROR, "Unknown exception in timer callback", config_);
                }
            }
        }

        // Sleep for 500ms or until notified/stopped
        std::unique_lock<std::mutex> cv_lock(timers_mutex_);
        timer_cv_.wait_for(cv_lock, std::chrono::milliseconds(500), [this]() {
            return !timers_running_;
        });
    }
}

bool cluster::is_owner(const std::string& user_id) const {
    if (!config_.owner_id.empty()) {
        return config_.owner_id == user_id;
    }
    return false;
}

bool cluster::is_server_owner(const std::string& server_id, const std::string& user_id) const {
    auto server = get_server(server_id);
    if (server) {
        return server->owner == user_id;
    }
    return false;
}

bool cluster::has_permission(const models::Server& server, const models::Member& member, int64_t permission_mask) const {
    if (server.owner == member.id) {
        return true;
    }
    int64_t allowed = 0;
    for (const auto& role_id : member.roles) {
        for (const auto& r : server.roles) {
            if (r.id == role_id) {
                if (r.permissions.is_number()) {
                    allowed |= r.permissions.get<int64_t>();
                } else if (r.permissions.is_object() && r.permissions.contains("server")) {
                    if (r.permissions["server"].is_number()) {
                        allowed |= r.permissions["server"].get<int64_t>();
                    }
                }
            }
        }
    }
    return (allowed & permission_mask) == permission_mask;
}

void cluster::on_rest_error(std::function<void(const std::string& method, const std::string& path, int status_code, const std::string& error_msg)> cb) {
    rest_error_handler_ = cb;
}

} // namespace stoatpp
