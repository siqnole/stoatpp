#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "role.h"

namespace stoatpp::models {

struct Server {
    std::string id;
    std::string owner;
    std::string name;
    std::optional<std::string> description;
    std::vector<std::string> channels;
    std::vector<Role> roles;
    std::optional<std::string> icon;
    std::optional<std::string> banner;
    int flags = 0;
    nlohmann::json raw;                            // original parsed object

    static Server from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline Server Server::from_json(const nlohmann::json& j) {
    Server s;
    s.raw = j;
    return s;
}

inline nlohmann::json Server::to_json() const {
    return raw;
}

} // namespace stoatpp::models
