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
    nlohmann::json raw;                            // original parsed object

    static Member from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Member Member::from_json(const nlohmann::json& j) {
    Member m;
    m.raw = j;
    return m;
}

inline nlohmann::json Member::to_json() const {
    return raw;
}

} // namespace stoatpp::models
