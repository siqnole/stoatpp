#include "stoatpp/cluster.h"
#include "stoatpp/utils/logger.h"
#include <thread>

namespace stoatpp {

void cluster::ban_user(const std::string& server_id,
                      const std::string& user_id,
                      const std::string& reason,
                      std::function<void(bool)> callback) {
    std::thread([this, server_id, user_id, reason, callback]() {
        try {
            auto res = rest_.ban_user(server_id, user_id,
                reason.empty() ? std::optional<std::string>{} : reason);
            if (res.success() && on_moderation_action) {
                on_moderation_action(user_id, "ban", reason);
            }
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in ban_user: " + std::string(e.what()), config_);
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
            if (res.success() && on_moderation_action) {
                on_moderation_action(user_id, "unban", "");
            }
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in unban_user: " + std::string(e.what()), config_);
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
            if (res.success() && on_moderation_action) {
                on_moderation_action(user_id, "kick", "");
            }
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in kick_member: " + std::string(e.what()), config_);
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
            if (res.success() && on_moderation_action) {
                on_moderation_action(user_id, "timeout", duration_iso);
            }
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in timeout_member: " + std::string(e.what()), config_);
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
            if (res.success() && on_moderation_action) {
                on_moderation_action(user_id, "untimeout", "");
            }
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in remove_timeout: " + std::string(e.what()), config_);
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
                if (callback) callback(models::Role::from_json(res.body), true);
            } else {
                if (callback) callback(models::Role{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in create_role: " + std::string(e.what()), config_);
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in delete_role: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::add_role_to_member(const std::string& server_id,
                                 const std::string& user_id,
                                 const std::string& role_id,
                                 std::function<void(bool, std::string)> callback) {
    std::thread([this, server_id, user_id, role_id, callback]() {
        try {
            auto get_res = rest_.get_server_member(server_id, user_id);
            if (!get_res.success()) {
                utils::logger::log(LogLevel::ERROR,
                    "add_role_to_member: GET member failed: " + get_res.body.dump(), config_);
                if (callback) callback(false, "GET_MEMBER_FAILED");
                return;
            }
            utils::logger::log(LogLevel::INFO,
                "add_role_to_member: GET member response: " + get_res.body.dump(), config_);
            auto mem = models::Member::from_json(get_res.body);
            bool has_role = false;
            for (const auto& r : mem.roles) {
                if (r == role_id) { has_role = true; break; }
            }
            if (has_role) {
                utils::logger::log(LogLevel::INFO,
                    "add_role_to_member: user already has role " + role_id, config_);
                if (callback) callback(true, "");
                return;
            }
            mem.roles.push_back(role_id);
            nlohmann::json fields;
            fields["roles"] = mem.roles;
            utils::logger::log(LogLevel::INFO,
                "add_role_to_member: PATCH payload: " + fields.dump(), config_);
            auto patch_res = rest_.edit_member(server_id, user_id, fields);
            std::string err_msg;
            if (!patch_res.success()) {
                if (patch_res.body.is_object() && patch_res.body.contains("type") && patch_res.body["type"].is_string()) {
                    err_msg = patch_res.body["type"].get<std::string>();
                } else if (patch_res.body.is_object() && patch_res.body.contains("error") && patch_res.body["error"].is_string()) {
                    err_msg = patch_res.body["error"].get<std::string>();
                } else {
                    err_msg = "HTTP_" + std::to_string(patch_res.status_code);
                }
            }
            if (callback) callback(patch_res.success(), err_msg);
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in add_role_to_member: " + std::string(e.what()), config_);
            if (callback) callback(false, e.what());
        }
    }).detach();
}

void cluster::add_role_to_member(const std::string& server_id,
                                 const std::string& user_id,
                                 const std::string& role_id,
                                 std::function<void(bool)> callback) {
    add_role_to_member(server_id, user_id, role_id, [callback](bool success, std::string) {
        if (callback) callback(success);
    });
}

void cluster::remove_role_from_member(const std::string& server_id,
                                      const std::string& user_id,
                                      const std::string& role_id,
                                      std::function<void(bool, std::string)> callback) {
    std::thread([this, server_id, user_id, role_id, callback]() {
        try {
            auto get_res = rest_.get_server_member(server_id, user_id);
            if (!get_res.success()) {
                utils::logger::log(LogLevel::ERROR,
                    "remove_role_from_member: GET member failed: " + get_res.body.dump(), config_);
                if (callback) callback(false, "GET_MEMBER_FAILED");
                return;
            }
            utils::logger::log(LogLevel::INFO,
                "remove_role_from_member: GET member response: " + get_res.body.dump(), config_);
            auto mem = models::Member::from_json(get_res.body);
            std::vector<std::string> new_roles;
            bool found = false;
            for (const auto& r : mem.roles) {
                if (r == role_id) { found = true; }
                else { new_roles.push_back(r); }
            }
            if (!found) {
                utils::logger::log(LogLevel::INFO,
                    "remove_role_from_member: user does not have role " + role_id, config_);
                if (callback) callback(true, "");
                return;
            }
            nlohmann::json fields;
            fields["roles"] = new_roles;
            utils::logger::log(LogLevel::INFO,
                "remove_role_from_member: PATCH payload: " + fields.dump(), config_);
            auto patch_res = rest_.edit_member(server_id, user_id, fields);
            std::string err_msg;
            if (!patch_res.success()) {
                if (patch_res.body.is_object() && patch_res.body.contains("type") && patch_res.body["type"].is_string()) {
                    err_msg = patch_res.body["type"].get<std::string>();
                } else if (patch_res.body.is_object() && patch_res.body.contains("error") && patch_res.body["error"].is_string()) {
                    err_msg = patch_res.body["error"].get<std::string>();
                } else {
                    err_msg = "HTTP_" + std::to_string(patch_res.status_code);
                }
            }
            if (callback) callback(patch_res.success(), err_msg);
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in remove_role_from_member: " + std::string(e.what()), config_);
            if (callback) callback(false, e.what());
        }
    }).detach();
}

void cluster::remove_role_from_member(const std::string& server_id,
                                      const std::string& user_id,
                                      const std::string& role_id,
                                      std::function<void(bool)> callback) {
    remove_role_from_member(server_id, user_id, role_id, [callback](bool success, std::string) {
        if (callback) callback(success);
    });
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in get_member_count: " + std::string(e.what()), config_);
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in fetch_server_invites: " + std::string(e.what()), config_);
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
                if (callback) callback(models::Invite::from_json(res.body), true);
            } else {
                if (callback) callback(models::Invite{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in create_invite: " + std::string(e.what()), config_);
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in set_slowmode: " + std::string(e.what()), config_);
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in clear_slowmode: " + std::string(e.what()), config_);
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in fetch_channel_permissions: " + std::string(e.what()), config_);
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
            utils::logger::log(LogLevel::ERROR,
                "Exception in set_channel_permissions: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::move_channel_to_category(const std::string& server_id,
                                       const std::string& channel_id,
                                       const std::string& category_id_or_name,
                                       std::function<void(bool)> callback) {
    std::thread([this, server_id, channel_id, category_id_or_name, callback]() {
        try {
            auto res = rest_.move_channel_to_category(server_id, channel_id, category_id_or_name);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in move_channel_to_category: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

} // namespace stoatpp
