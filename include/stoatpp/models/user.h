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
    return u;
}

inline nlohmann::json User::to_json() const {
    return raw;
}

} // namespace stoatpp::models
