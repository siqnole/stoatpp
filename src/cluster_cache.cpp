#include "stoatpp/cluster.h"
#include "stoatpp/utils/logger.h"

namespace stoatpp {

// ---------------------------------------------------------------------------
// Cache listener wiring (called from constructor)
// ---------------------------------------------------------------------------

void cluster::setup_cache_listeners() {
    this->on_server_member_join([this](const events::ServerMemberJoin& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        if (!e.member.server_id.empty() && !e.member.id.empty()) {
            member_cache_[e.member.server_id][e.member.id] = e.member;
        }
    });

    this->on_server_member_leave([this](const events::ServerMemberLeave& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        if (!e.server_id.empty() && !e.user_id.empty()) {
            member_cache_[e.server_id].erase(e.user_id);
        }
    });

    this->on_ready([this](const events::Ready& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG,
            "Caching Ready data: user=" + e.user.username +
            ", servers=" + std::to_string(e.servers.size()) +
            ", channels=" + std::to_string(e.channels.size()), config_);
        current_user_ = e.user;
        user_cache_[e.user.id] = e.user;

        // Cache ALL users from Ready — their full objects include correct bot flags.
        // This is the authoritative source for is-bot detection since Revolt omits
        // the "bot" field from per-message user objects.
        for (const auto& u : e.users) {
            if (!u.id.empty()) {
                user_cache_[u.id] = u;
            }
        }

        for (const auto& s : e.servers) {
            server_cache_[s.id] = s;
        }
        for (const auto& c : e.channels) {
            channel_cache_[c.id] = c;
        }
        for (const auto& m : e.members) {
            if (!m.server_id.empty() && !m.id.empty()) {
                member_cache_[m.server_id][m.id] = m;
            }
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
        utils::logger::log(LogLevel::DEBUG,
            "Caching new server: " + e.server.name + " (" + e.server.id + ")", config_);
        server_cache_[e.server.id] = e.server;

        if (e.raw.contains("channels") && e.raw["channels"].is_array()) {
            for (const auto& c_json : e.raw["channels"]) {
                auto c = models::Channel::from_json(c_json);
                channel_cache_[c.id] = c;
            }
        }
    });

    this->on_server_delete([this](const events::ServerDelete& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        utils::logger::log(LogLevel::DEBUG, "Removing server from cache: " + e.id, config_);
        server_cache_.erase(e.id);
    });

    this->on_server_update([this](const events::ServerUpdate& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        auto it = server_cache_.find(e.id);
        if (it != server_cache_.end()) {
            utils::logger::log(LogLevel::DEBUG,
                "Updating server in cache: " + it->second.name + " (" + e.id + ")", config_);
            if (e.data.contains("name") && e.data["name"].is_string()) {
                it->second.name = e.data["name"].get<std::string>();
            }
            if (e.data.contains("owner") && e.data["owner"].is_string()) {
                it->second.owner = e.data["owner"].get<std::string>();
            }
            for (auto& [key, val] : e.data.items()) {
                it->second.raw[key] = val;
            }
            for (const auto& field : e.clear) {
                it->second.raw.erase(field);
            }
        }
    });

    this->on_server_role_create([this](const events::ServerRoleCreate& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        auto it = server_cache_.find(e.server_id);
        if (it != server_cache_.end()) {
            auto new_role = e.role;
            if (new_role.id.empty() && !e.role_id.empty()) new_role.id = e.role_id;
            utils::logger::log(LogLevel::DEBUG,
                "Caching new role: " + new_role.name + " (" + new_role.id +
                ") in server " + e.server_id, config_);
            bool found = false;
            for (auto& r : it->second.roles) {
                if (r.id == new_role.id || (!r.id.empty() && r.id == e.role_id)) {
                    r = new_role;
                    found = true;
                    break;
                }
            }
            if (!found) {
                it->second.roles.push_back(new_role);
            }
        }
    });

    this->on_server_role_update([this](const events::ServerRoleUpdate& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        auto it = server_cache_.find(e.server_id);
        if (it != server_cache_.end()) {
            for (auto& r : it->second.roles) {
                if (r.id == e.role_id) {
                    utils::logger::log(LogLevel::DEBUG,
                        "Updating role cache: " + r.name + " (" + e.role_id +
                        ") in server " + e.server_id, config_);
                    if (e.data.contains("name") && e.data["name"].is_string()) {
                        r.name = e.data["name"].get<std::string>();
                    }
                    if (e.data.contains("colour") && !e.data["colour"].is_null()) {
                        if (e.data["colour"].is_string()) {
                            r.colour = e.data["colour"].get<std::string>();
                        }
                    }
                    if (e.data.contains("rank") && e.data["rank"].is_number()) {
                        r.rank = e.data["rank"].get<int>();
                    }
                    if (e.data.contains("permissions")) {
                        r.permissions = e.data["permissions"];
                    }
                    for (auto& [key, val] : e.data.items()) {
                        r.raw[key] = val;
                    }
                    for (const auto& field : e.clear) {
                        r.raw.erase(field);
                    }
                    break;
                }
            }
        }
    });

    this->on_server_role_delete([this](const events::ServerRoleDelete& e) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        auto it = server_cache_.find(e.server_id);
        if (it != server_cache_.end()) {
            utils::logger::log(LogLevel::DEBUG,
                "Deleting role from cache: " + e.role_id + " in server " + e.server_id, config_);
            auto& roles = it->second.roles;
            roles.erase(std::remove_if(roles.begin(), roles.end(),
                [&e](const auto& r) { return r.id == e.role_id; }), roles.end());
        }
    });
}

// ---------------------------------------------------------------------------
// Cache read accessors
// ---------------------------------------------------------------------------

std::optional<models::Server> cluster::get_server(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = server_cache_.find(id);
    if (it != server_cache_.end()) return it->second;
    return std::nullopt;
}

std::vector<models::Server> cluster::get_servers() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    std::vector<models::Server> servers;
    for (const auto& [id, srv] : server_cache_) {
        servers.push_back(srv);
    }
    return servers;
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

void cluster::cache_server(const models::Server& srv) {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    server_cache_[srv.id] = srv;
}

void cluster::cache_user(const models::User& user) {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    if (!user.id.empty()) {
        user_cache_[user.id] = user;
    }
}

void cluster::cache_member(const models::Member& member) {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    if (!member.server_id.empty() && !member.id.empty()) {
        member_cache_[member.server_id][member.id] = member;
    }
}

models::User cluster::current_user() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return current_user_;
}

std::optional<models::Member> cluster::get_member(const std::string& server_id, const std::string& user_id) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto s_it = member_cache_.find(server_id);
    if (s_it != member_cache_.end()) {
        auto m_it = s_it->second.find(user_id);
        if (m_it != s_it->second.end()) {
            return m_it->second;
        }
    }
    return std::nullopt;
}

size_t cluster::get_member_count_sync(const std::string& server_id) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto s_it = member_cache_.find(server_id);
    if (s_it != member_cache_.end()) {
        return s_it->second.size();
    }
    return 0;
}

size_t cluster::get_total_member_count() {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    size_t total = 0;
    for (const auto& kv : member_cache_) {
        total += kv.second.size();
    }
    return total;
}

} // namespace stoatpp
