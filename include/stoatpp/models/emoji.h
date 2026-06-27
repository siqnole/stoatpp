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
    return e;
}

inline nlohmann::json Emoji::to_json() const {
    return raw;
}

} // namespace stoatpp::models
