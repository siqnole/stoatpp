#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Role {
    std::string id;
    std::string name;
    std::optional<std::string> colour;
    int rank = 0;
    nlohmann::json permissions;                    // allow/deny object
    nlohmann::json raw;                            // original parsed object

    static Role from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Role Role::from_json(const nlohmann::json& j) {
    Role r;
    r.raw = j;
    return r;
}

inline nlohmann::json Role::to_json() const {
    return raw;
}

} // namespace stoatpp::models
