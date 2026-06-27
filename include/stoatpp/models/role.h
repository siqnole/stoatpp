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
    if (j.contains("id") && j["id"].is_string()) r.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) r.id = j["_id"].get<std::string>();
    if (j.contains("name") && j["name"].is_string()) r.name = j["name"].get<std::string>();
    if (j.contains("colour") && j["colour"].is_string()) r.colour = j["colour"].get<std::string>();
    if (j.contains("rank") && j["rank"].is_number()) r.rank = j["rank"].get<int>();
    if (j.contains("permissions")) r.permissions = j["permissions"];
    return r;
}

inline nlohmann::json Role::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["name"] = name;
    if (colour) j["colour"] = *colour;
    j["rank"] = rank;
    j["permissions"] = permissions;
    return j;
}

} // namespace stoatpp::models
