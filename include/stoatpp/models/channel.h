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
    return c;
}

inline nlohmann::json Channel::to_json() const {
    return raw;
}

} // namespace stoatpp::models
