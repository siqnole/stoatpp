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
    std::optional<std::string> timeout;            // ISO timestamp representing when the member's timeout ends
    nlohmann::json raw;                            // original parsed object

    static Member from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Member Member::from_json(const nlohmann::json& j) {
    Member m;
    m.raw = j;
    
    nlohmann::json id_val;
    if (j.contains("id")) id_val = j["id"];
    else if (j.contains("_id")) id_val = j["_id"];

    if (!id_val.is_null()) {
        if (id_val.is_object()) {
            if (id_val.contains("user") && id_val["user"].is_string()) {
                m.id = id_val["user"].get<std::string>();
            }
            if (id_val.contains("server") && id_val["server"].is_string()) {
                m.server_id = id_val["server"].get<std::string>();
            }
        } else if (id_val.is_string()) {
            m.id = id_val.get<std::string>();
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
    if (j.contains("timeout") && j["timeout"].is_string()) {
        m.timeout = j["timeout"].get<std::string>();
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
    if (timeout) j["timeout"] = *timeout;
    return j;
}

} // namespace stoatpp::models
