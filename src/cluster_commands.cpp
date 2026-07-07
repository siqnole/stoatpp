#include "stoatpp/cluster.h"
#include "stoatpp/bot_module.h"
#include "stoatpp/utils/logger.h"
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace stoatpp {

// ---------------------------------------------------------------------------
// File-local helpers (formerly static in cluster.cpp)
// ---------------------------------------------------------------------------

static std::string cmd_to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return str;
}

static int levenshtein_distance(const std::string& s1, const std::string& s2) {
    int len1 = s1.size(), len2 = s2.size();
    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));
    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            d[i][j] = std::min({d[i-1][j]+1, d[i][j-1]+1, d[i-1][j-1]+cost});
        }
    }
    return d[len1][len2];
}

// ---------------------------------------------------------------------------
// Default help command + message-listener wiring (called from constructor)
// ---------------------------------------------------------------------------

void cluster::setup_default_help() {
    register_command("help", [this](cluster& bot, const events::Message& msg,
                                   const std::vector<std::string>& /*args*/) {
        std::vector<Command> cmds = registered_commands_;
        std::sort(cmds.begin(), cmds.end(),
            [](const Command& a, const Command& b) { return a.name < b.name; });

        std::string prefix = config_.command_prefix;
        if (config_.prefix_resolver) {
            auto resolved = config_.prefix_resolver(bot, msg);
            if (!resolved.empty()) prefix = resolved[0];
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
                    desc += "`" + prefix + cmd.aliases[a] + "`" +
                            (a + 1 < cmd.aliases.size() ? ", " : "");
                }
                desc += ")";
            }
            desc += "\n";
            std::string syntax = prefix + cmd.name;
            if (!cmd.args.empty()) {
                for (const auto& arg : cmd.args) syntax += " " + arg;
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

        bot.send_message(msg.channel_id, payload,
            [this, msg, max_pages](stoatpp::models::Message res, bool success) {
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
        if (it == help_sessions_.end()) return;

        auto& session = it->second;
        if (e.user_id == current_user().id) return;
        if (e.user_id != session.user_id) {
            lock.unlock();
            unreact_from_message(e.channel_id, e.id, e.emoji_id, e.user_id);
            return;
        }

        bool page_changed = false;
        if (e.emoji_id == "⬅️" || e.emoji_id == "◀" || e.emoji_id == "⬅") {
            session.current_page = (session.current_page > 0)
                ? session.current_page - 1 : session.max_pages - 1;
            page_changed = true;
        } else if (e.emoji_id == "➡️" || e.emoji_id == "▶" || e.emoji_id == "➡") {
            session.current_page = (session.current_page < session.max_pages - 1)
                ? session.current_page + 1 : 0;
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
                if (!resolved.empty()) prefix = resolved[0];
            }

            std::vector<Command> cmds = registered_commands_;
            std::sort(cmds.begin(), cmds.end(),
                [](const Command& a, const Command& b) { return a.name < b.name; });

            stoatpp::models::MessagePayload payload;
            payload.content = " ";
            nlohmann::json embed = nlohmann::json::object();
            embed["colour"] = config_.default_help_color;
            embed["title"] = "stoat++ default help menu (page " +
                             std::to_string(page + 1) + " of " + std::to_string(max_pages) + ")";

            std::string desc = "here is a list of all registered commands:\n\n";
            size_t start = page * 3;
            for (size_t i = start; i < start + 3 && i < cmds.size(); ++i) {
                const auto& cmd = cmds[i];
                desc += "• **" + prefix + cmd.name + "**";
                if (!cmd.aliases.empty()) {
                    desc += " (aliases: ";
                    for (size_t a = 0; a < cmd.aliases.size(); ++a) {
                        desc += "`" + prefix + cmd.aliases[a] + "`" +
                                (a + 1 < cmd.aliases.size() ? ", " : "");
                    }
                    desc += ")";
                }
                desc += "\n";
                std::string syntax = prefix + cmd.name;
                if (!cmd.args.empty()) {
                    for (const auto& arg : cmd.args) syntax += " " + arg;
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
    });
}

void cluster::setup_message_listener() {
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
        if (cb) { cb(msg); return; }
        dispatch_command(msg);
    });

    this->on_message_update([this](const events::MessageUpdate& e) {
        if (!config_.dispatch_commands_on_edit) return;
        if (e.data.contains("content") && e.data["content"].is_string()) {
            std::thread([this, e]() {
                try {
                    auto res = rest_.get_message(e.channel_id, e.id);
                    if (res.success()) {
                        auto msg = events::Message::from_json(res.body);
                        if (msg.server_id.empty() && !msg.channel_id.empty()) {
                            auto chan = get_channel(msg.channel_id);
                            if (chan && chan->server) {
                                msg.server_id = *(chan->server);
                            } else {
                                auto chan_res = rest_.get_channel(msg.channel_id);
                                if (chan_res.success()) {
                                    auto fetched = models::Channel::from_json(chan_res.body);
                                    if (fetched.server) msg.server_id = *(fetched.server);
                                }
                            }
                        }
                        dispatch_command(msg);
                    }
                } catch (const std::exception& err) {
                    utils::logger::log(LogLevel::ERROR,
                        "Exception in message update fetch: " + std::string(err.what()), config_);
                }
            }).detach();
        }
    });
}

// ---------------------------------------------------------------------------
// Command registration
// ---------------------------------------------------------------------------

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
                              std::function<void(cluster&, const events::Message&,
                                                 const std::vector<std::string>&)> cb) {
    Command cmd;
    cmd.name = name;
    cmd.callback = cb;
    register_command(cmd);
}

void cluster::register_command(const std::string& name,
                              const std::vector<std::string>& aliases,
                              std::function<void(cluster&, const events::Message&,
                                                 const std::vector<std::string>&)> cb) {
    Command cmd;
    cmd.name = name;
    cmd.aliases = aliases;
    cmd.callback = cb;
    register_command(cmd);
}

void cluster::on_command_cooldown(
    std::function<void(cluster&, const events::Message&, const Command&, int64_t)> cb) {
    command_cooldown_handler_ = cb;
}

void cluster::on_command_run(
    std::function<bool(cluster&, const events::Message&, const Command&,
                       const std::optional<models::Member>&, const std::vector<std::string>&)> cb) {
    command_run_handler_ = cb;
}

void cluster::on_command_error(
    std::function<void(cluster&, const events::Message&, const Command&, const std::string&)> cb) {
    command_error_handler_ = cb;
}

void cluster::set_message_preprocessor(std::function<void(cluster&, events::Message&)> cb) {
    message_preprocessor_ = cb;
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

void cluster::dispatch_command(events::Message msg) {
    if (message_preprocessor_) {
        message_preprocessor_(*this, msg);
    }

    if (msg.server_id.empty() && !msg.channel_id.empty()) {
        auto chan = get_channel(msg.channel_id);
        if (chan && chan->server) msg.server_id = *(chan->server);
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
    std::sort(prefixes.begin(), prefixes.end(),
        [](const std::string& a, const std::string& b) { return a.size() > b.size(); });

    std::string matched_prefix;
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
    while (ss >> arg) args.push_back(arg);

    Command cmd_obj;
    bool found_cmd = false;
    std::string match_cmd = cmd;
    if (config_.case_insensitive_commands) {
        std::transform(match_cmd.begin(), match_cmd.end(), match_cmd.begin(), ::tolower);
    }

    for (const auto& c : registered_commands_) {
        std::string c_name = c.name;
        if (config_.case_insensitive_commands)
            std::transform(c_name.begin(), c_name.end(), c_name.begin(), ::tolower);
        if (c_name == match_cmd) { cmd_obj = c; found_cmd = true; break; }
        for (const auto& a : c.aliases) {
            std::string a_name = a;
            if (config_.case_insensitive_commands)
                std::transform(a_name.begin(), a_name.end(), a_name.begin(), ::tolower);
            if (a_name == match_cmd) { cmd_obj = c; found_cmd = true; break; }
        }
        if (found_cmd) break;
    }

    if (found_cmd) {
        events::Message dispatched_msg = msg;
        dispatched_msg.prefix = matched_prefix;

        if (cmd_obj.cooldown_seconds > 0) {
            std::unique_lock<std::mutex> lock(cooldowns_mutex_);
            auto now = std::chrono::steady_clock::now();
            auto& cmd_cooldowns = command_cooldowns_[cmd_obj.name];
            auto it = cmd_cooldowns.find(msg.author.id);
            if (it != cmd_cooldowns.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second).count();
                if (elapsed < cmd_obj.cooldown_seconds) {
                    int64_t remaining = cmd_obj.cooldown_seconds - elapsed;
                    lock.unlock();
                    if (command_cooldown_handler_)
                        command_cooldown_handler_(*this, dispatched_msg, cmd_obj, remaining);
                    return;
                }
            }
            cmd_cooldowns[msg.author.id] = now;
        }

        if (!dispatched_msg.server_id.empty()) {
            fetch_member(dispatched_msg.server_id, dispatched_msg.author.id,
                [this, dispatched_msg, args, cmd_obj](models::Member mem, bool success) {
                    if (!success) {
                        if (command_error_handler_)
                            command_error_handler_(*this, dispatched_msg, cmd_obj, "member_fetch_failed");
                        return;
                    }
                    if (command_run_handler_) {
                        if (!command_run_handler_(*this, dispatched_msg, cmd_obj, mem, args)) return;
                    }
                    if (cmd_obj.required_permissions > 0) {
                        auto server = get_server(dispatched_msg.server_id);
                        if (!server) {
                            if (command_error_handler_)
                                command_error_handler_(*this, dispatched_msg, cmd_obj, "server_resolve_failed");
                            return;
                        }
                        if (!has_permission(*server, mem, cmd_obj.required_permissions)) {
                            if (command_error_handler_)
                                command_error_handler_(*this, dispatched_msg, cmd_obj, "permission_denied");
                            return;
                        }
                    }
                    commands_executed_++;
                    cmd_obj.callback(*this, dispatched_msg, args);
                });
        } else {
            if (command_run_handler_) {
                if (!command_run_handler_(*this, dispatched_msg, cmd_obj, std::nullopt, args)) return;
            }
            if (cmd_obj.required_permissions > 0) {
                if (command_error_handler_)
                    command_error_handler_(*this, dispatched_msg, cmd_obj, "server_only");
                return;
            }
            commands_executed_++;
            cmd_obj.callback(*this, dispatched_msg, args);
        }
    } else {
        if (cmd.length() > 2) {
            std::string lower_typed = cmd_to_lower(cmd);
            double max_score = -1.0;
            std::vector<std::pair<std::string, double>> candidates;

            for (const auto& c : registered_commands_) {
                double best = -1.0;
                std::string lower_name = cmd_to_lower(c.name);
                int dist = levenshtein_distance(lower_typed, lower_name);
                double score = 1.0 - static_cast<double>(dist) /
                               std::max(lower_name.length(), lower_typed.length());
                if (score > best) best = score;
                for (const auto& a : c.aliases) {
                    std::string lower_alias = cmd_to_lower(a);
                    int ad = levenshtein_distance(lower_typed, lower_alias);
                    double as = 1.0 - static_cast<double>(ad) /
                                std::max(lower_alias.length(), lower_typed.length());
                    if (as > best) best = as;
                }
                if (best >= 0.5) {
                    candidates.push_back({c.name, best});
                    if (best > max_score) max_score = best;
                }
            }

            if (max_score >= 0.5) {
                std::vector<std::string> suggestions;
                for (const auto& pair : candidates) {
                    if (pair.second >= max_score - 0.001)
                        suggestions.push_back(pair.first);
                }
                if (!suggestions.empty()) {
                    std::sort(suggestions.begin(), suggestions.end());
                    std::string suggestion_str;
                    if (suggestions.size() == 1) {
                        suggestion_str = "did you mean **" + suggestions[0] + "**?";
                    } else {
                        std::string list_str;
                        for (size_t i = 0; i < suggestions.size(); ++i) {
                            if (i > 0) list_str += (i == suggestions.size()-1) ? " or " : ", ";
                            list_str += suggestions[i];
                        }
                        suggestion_str = "did you mean **" + list_str + "**?";
                    }
                    send_message(msg.channel_id, suggestion_str);
                }
            }
        }
    }
}

} // namespace stoatpp
