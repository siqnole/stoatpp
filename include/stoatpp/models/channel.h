#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Channel {
    std::string id;
    std::string channel_type;                      // "TextChannel", "DirectMessage", etc.
    std::optional<std::string> server;
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> icon;
    bool nsfw = false;
    std::optional<nlohmann::json> default_permissions;
    std::optional<nlohmann::json> role_permissions;
    nlohmann::json raw;                            // original parsed object

    static Channel from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Channel Channel::from_json(const nlohmann::json& j) {
    Channel c;
    c.raw = j;
    if (j.contains("id") && j["id"].is_string()) c.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) c.id = j["_id"].get<std::string>();
    if (j.contains("channel_type") && j["channel_type"].is_string()) c.channel_type = j["channel_type"].get<std::string>();
    if (j.contains("server") && j["server"].is_string()) c.server = j["server"].get<std::string>();
    if (j.contains("name") && j["name"].is_string()) c.name = j["name"].get<std::string>();
    if (j.contains("description") && j["description"].is_string()) c.description = j["description"].get<std::string>();
    if (j.contains("icon") && !j["icon"].is_null()) {
        if (j["icon"].is_string()) {
            c.icon = j["icon"].get<std::string>();
        } else if (j["icon"].is_object() && j["icon"].contains("id") && j["icon"]["id"].is_string()) {
            c.icon = j["icon"]["id"].get<std::string>();
        }
    }
    if (j.contains("nsfw") && j["nsfw"].is_boolean()) c.nsfw = j["nsfw"].get<bool>();
    if (j.contains("default_permissions")) c.default_permissions = j["default_permissions"];
    if (j.contains("role_permissions")) c.role_permissions = j["role_permissions"];
    return c;
}

inline nlohmann::json Channel::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["channel_type"] = channel_type;
    if (server) j["server"] = *server;
    if (name) j["name"] = *name;
    if (description) j["description"] = *description;
    if (icon) j["icon"] = *icon;
    j["nsfw"] = nsfw;
    if (default_permissions) j["default_permissions"] = *default_permissions;
    if (role_permissions) j["role_permissions"] = *role_permissions;
    return j;
}

} // namespace stoatpp::models
