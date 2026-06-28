#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "role.h"
#include "member.h"

namespace stoatpp::models {

struct Server {
    std::string id;
    std::string owner;
    std::string name;
    std::optional<std::string> description;
    std::vector<std::string> channels;
    std::vector<Role> roles;
    std::optional<std::string> icon;
    std::optional<std::string> banner;
    int flags = 0;
    nlohmann::json raw;                            // original parsed object

    static Server from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    int get_highest_rank(const Member& member) const {
        if (owner == member.id) {
            return -1;
        }
        int highest = 999999;
        for (const auto& r_id : member.roles) {
            for (const auto& r : roles) {
                if (r.id == r_id) {
                    if (r.rank < highest) {
                        highest = r.rank;
                    }
                }
            }
        }
        return highest;
    }

    bool check_hierarchy(const Member& a, const Member& b) const {
        int rank_a = get_highest_rank(a);
        int rank_b = get_highest_rank(b);
        return rank_a < rank_b;
    }
};

inline Server Server::from_json(const nlohmann::json& j) {
    Server s;
    s.raw = j;
    if (j.contains("id") && j["id"].is_string()) s.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) s.id = j["_id"].get<std::string>();
    if (j.contains("owner") && j["owner"].is_string()) s.owner = j["owner"].get<std::string>();
    if (j.contains("name") && j["name"].is_string()) s.name = j["name"].get<std::string>();
    if (j.contains("description") && j["description"].is_string()) s.description = j["description"].get<std::string>();
    if (j.contains("channels") && j["channels"].is_array()) {
        for (const auto& ch : j["channels"]) {
            if (ch.is_string()) s.channels.push_back(ch.get<std::string>());
        }
    }
    if (j.contains("roles")) {
        if (j["roles"].is_array()) {
            for (const auto& r : j["roles"]) {
                s.roles.push_back(Role::from_json(r));
            }
        } else if (j["roles"].is_object()) {
            for (auto& [key, value] : j["roles"].items()) {
                nlohmann::json role_json = value;
                role_json["id"] = key;
                s.roles.push_back(Role::from_json(role_json));
            }
        }
    }
    if (j.contains("icon") && !j["icon"].is_null()) {
        if (j["icon"].is_string()) {
            s.icon = j["icon"].get<std::string>();
        } else if (j["icon"].is_object() && j["icon"].contains("id") && j["icon"]["id"].is_string()) {
            s.icon = j["icon"]["id"].get<std::string>();
        }
    }
    if (j.contains("banner") && !j["banner"].is_null()) {
        if (j["banner"].is_string()) {
            s.banner = j["banner"].get<std::string>();
        } else if (j["banner"].is_object() && j["banner"].contains("id") && j["banner"]["id"].is_string()) {
            s.banner = j["banner"]["id"].get<std::string>();
        }
    }
    if (j.contains("flags") && j["flags"].is_number()) s.flags = j["flags"].get<int>();
    return s;
}

inline nlohmann::json Server::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["owner"] = owner;
    j["name"] = name;
    if (description) j["description"] = *description;
    j["channels"] = channels;
    
    nlohmann::json roles_json = nlohmann::json::object();
    for (const auto& r : roles) {
        roles_json[r.id] = r.to_json();
    }
    j["roles"] = roles_json;
    
    if (icon) j["icon"] = *icon;
    if (banner) j["banner"] = *banner;
    j["flags"] = flags;
    return j;
}

} // namespace stoatpp::models
