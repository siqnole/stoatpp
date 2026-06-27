#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Ban {
    std::string user_id;
    std::optional<std::string> reason;

    static Ban from_json(const nlohmann::json& j) {
        Ban b;
        if (j.contains("id") && j["id"].is_object()) {
            if (j["id"].contains("user") && j["id"]["user"].is_string()) {
                b.user_id = j["id"]["user"].get<std::string>();
            }
        } else if (j.contains("_id") && j["_id"].is_object()) {
            if (j["_id"].contains("user") && j["_id"]["user"].is_string()) {
                b.user_id = j["_id"]["user"].get<std::string>();
            }
        }
        if (j.contains("reason") && j["reason"].is_string()) b.reason = j["reason"].get<std::string>();
        return b;
    }
};

} // namespace stoatpp::models
