#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Emoji {
    std::string id;
    std::string name;
    std::optional<std::string> server_id;         // null = global
    std::string creator_id;
    bool animated = false;
    nlohmann::json raw;                            // original parsed object

    static Emoji from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Emoji Emoji::from_json(const nlohmann::json& j) {
    Emoji e;
    e.raw = j;
    if (j.contains("id") && j["id"].is_string()) e.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) e.id = j["_id"].get<std::string>();
    if (j.contains("name") && j["name"].is_string()) e.name = j["name"].get<std::string>();
    if (j.contains("server_id") && j["server_id"].is_string()) e.server_id = j["server_id"].get<std::string>();
    if (j.contains("creator_id") && j["creator_id"].is_string()) e.creator_id = j["creator_id"].get<std::string>();
    if (j.contains("animated") && j["animated"].is_boolean()) e.animated = j["animated"].get<bool>();
    return e;
}

inline nlohmann::json Emoji::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["name"] = name;
    if (server_id) j["server_id"] = *server_id;
    j["creator_id"] = creator_id;
    j["animated"] = animated;
    return j;
}

} // namespace stoatpp::models
