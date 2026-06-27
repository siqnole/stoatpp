#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct User {
    std::string id;
    std::string username;
    std::string discriminator;
    std::optional<std::string> display_name;
    std::optional<std::string> avatar;             // attachment ID
    std::optional<std::string> banner;             // banner attachment ID
    std::optional<std::string> bio;
    std::optional<std::string> status_text;
    std::optional<std::string> presence;
    bool bot = false;
    int flags = 0;
    nlohmann::json raw;                            // original parsed object

    static User from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    std::string avatar_url(const std::string& cdn_url = "https://cdn.stoatusercontent.com") const;
    std::string banner_url(const std::string& cdn_url = "https://cdn.stoatusercontent.com") const;
};

inline User User::from_json(const nlohmann::json& j) {
    User u;
    u.raw = j;
    if (j.contains("id") && j["id"].is_string()) u.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) u.id = j["_id"].get<std::string>();
    
    if (j.contains("username") && j["username"].is_string()) u.username = j["username"].get<std::string>();
    if (j.contains("discriminator") && j["discriminator"].is_string()) u.discriminator = j["discriminator"].get<std::string>();
    if (j.contains("display_name") && j["display_name"].is_string()) u.display_name = j["display_name"].get<std::string>();
    
    if (j.contains("avatar") && !j["avatar"].is_null()) {
        if (j["avatar"].is_string()) {
            u.avatar = j["avatar"].get<std::string>();
        } else if (j["avatar"].is_object()) {
            if (j["avatar"].contains("id") && j["avatar"]["id"].is_string()) {
                u.avatar = j["avatar"]["id"].get<std::string>();
            } else if (j["avatar"].contains("_id") && j["avatar"]["_id"].is_string()) {
                u.avatar = j["avatar"]["_id"].get<std::string>();
            }
        }
    }

    if (j.contains("profile") && j["profile"].is_object()) {
        const auto& prof = j["profile"];
        if (prof.contains("background")) {
            if (prof["background"].is_string()) {
                u.banner = prof["background"].get<std::string>();
            } else if (prof["background"].is_object()) {
                if (prof["background"].contains("id") && prof["background"]["id"].is_string()) {
                    u.banner = prof["background"]["id"].get<std::string>();
                } else if (prof["background"].contains("_id") && prof["background"]["_id"].is_string()) {
                    u.banner = prof["background"]["_id"].get<std::string>();
                }
            }
        }
        if (prof.contains("content") && prof["content"].is_string()) {
            u.bio = prof["content"].get<std::string>();
        }
    }

    if (j.contains("status") && j["status"].is_object()) {
        const auto& stat = j["status"];
        if (stat.contains("text") && stat["text"].is_string()) {
            u.status_text = stat["text"].get<std::string>();
        }
        if (stat.contains("presence") && stat["presence"].is_string()) {
            u.presence = stat["presence"].get<std::string>();
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
    j["discriminator"] = discriminator;
    if (display_name) j["display_name"] = *display_name;
    if (avatar) j["avatar"] = *avatar;
    j["bot"] = bot;
    j["flags"] = flags;
    
    nlohmann::json prof = nlohmann::json::object();
    if (banner) prof["background"] = *banner;
    if (bio) prof["content"] = *bio;
    if (!prof.empty()) {
        j["profile"] = prof;
    }

    nlohmann::json stat = nlohmann::json::object();
    if (status_text) stat["text"] = *status_text;
    if (presence) stat["presence"] = *presence;
    if (!stat.empty()) {
        j["status"] = stat;
    }
    return j;
}

inline std::string User::avatar_url(const std::string& cdn_url) const {
    if (avatar) {
        return cdn_url + "/avatars/" + *avatar;
    }
    return "https://api.stoat.chat/assets/default_avatar.png";
}

inline std::string User::banner_url(const std::string& cdn_url) const {
    if (banner) {
        return cdn_url + "/backgrounds/" + *banner;
    }
    return "";
}

} // namespace stoatpp::models
