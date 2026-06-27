#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Member {
    std::string id;                                // user ID
    std::string server_id;
    std::optional<std::string> nickname;
    std::optional<std::string> avatar;
    std::vector<std::string> roles;
    std::string joined_at;
    nlohmann::json raw;                            // original parsed object

    static Member from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Member Member::from_json(const nlohmann::json& j) {
    Member m;
    m.raw = j;
    
    if (j.contains("id")) {
        if (j["id"].is_object()) {
            if (j["id"].contains("user") && j["id"]["user"].is_string()) {
                m.id = j["id"]["user"].get<std::string>();
            }
            if (j["id"].contains("server") && j["id"]["server"].is_string()) {
                m.server_id = j["id"]["server"].get<std::string>();
            }
        } else if (j["id"].is_string()) {
            m.id = j["id"].get<std::string>();
        }
    }
    
    if (j.contains("user_id") && j["user_id"].is_string()) m.id = j["user_id"].get<std::string>();
    if (j.contains("server_id") && j["server_id"].is_string()) m.server_id = j["server_id"].get<std::string>();
    
    if (j.contains("nickname") && j["nickname"].is_string()) m.nickname = j["nickname"].get<std::string>();
    if (j.contains("avatar") && !j["avatar"].is_null()) {
        if (j["avatar"].is_string()) {
            m.avatar = j["avatar"].get<std::string>();
        } else if (j["avatar"].is_object()) {
            if (j["avatar"].contains("id") && j["avatar"]["id"].is_string()) {
                m.avatar = j["avatar"]["id"].get<std::string>();
            } else if (j["avatar"].contains("_id") && j["avatar"]["_id"].is_string()) {
                m.avatar = j["avatar"]["_id"].get<std::string>();
            }
        }
    }
    if (j.contains("roles") && j["roles"].is_array()) {
        for (const auto& r : j["roles"]) {
            if (r.is_string()) m.roles.push_back(r.get<std::string>());
        }
    }
    if (j.contains("joined_at") && j["joined_at"].is_string()) {
        m.joined_at = j["joined_at"].get<std::string>();
    }
    return m;
}

inline nlohmann::json Member::to_json() const {
    nlohmann::json j = raw;
    j["user_id"] = id;
    j["server_id"] = server_id;
    if (nickname) j["nickname"] = *nickname;
    if (avatar) j["avatar"] = *avatar;
    j["roles"] = roles;
    if (!joined_at.empty()) j["joined_at"] = joined_at;
    return j;
}

} // namespace stoatpp::models
