#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct User {
    std::string id;
    std::string username;
    std::optional<std::string> display_name;
    std::optional<std::string> avatar;             // attachment ID
    bool bot = false;
    int flags = 0;
    nlohmann::json raw;                            // original parsed object

    static User from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline User User::from_json(const nlohmann::json& j) {
    User u;
    u.raw = j;
    if (j.contains("id") && j["id"].is_string()) u.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) u.id = j["_id"].get<std::string>(); // Revolt compatibility
    
    if (j.contains("username") && j["username"].is_string()) u.username = j["username"].get<std::string>();
    if (j.contains("display_name") && j["display_name"].is_string()) u.display_name = j["display_name"].get<std::string>();
    
    if (j.contains("avatar") && !j["avatar"].is_null()) {
        if (j["avatar"].is_string()) {
            u.avatar = j["avatar"].get<std::string>();
        } else if (j["avatar"].is_object() && j["avatar"].contains("id") && j["avatar"]["id"].is_string()) {
            u.avatar = j["avatar"]["id"].get<std::string>();
        }
    }
    if (j.contains("bot") && j["bot"].is_boolean()) u.bot = j["bot"].get<bool>();
    if (j.contains("flags") && j["flags"].is_number()) u.flags = j["flags"].get<int>();
    return u;
}

inline nlohmann::json User::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["username"] = username;
    if (display_name) j["display_name"] = *display_name;
    if (avatar) j["avatar"] = *avatar;
    j["bot"] = bot;
    j["flags"] = flags;
    return j;
}

} // namespace stoatpp::models
