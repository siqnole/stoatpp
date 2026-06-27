#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Bot {
    std::string id;
    std::string owner_id;
    std::string name;
    std::optional<std::string> description;
    std::string token;
    bool public_bot = false;

    static Bot from_json(const nlohmann::json& j) {
        Bot b;
        if (j.contains("id") && j["id"].is_string()) b.id = j["id"].get<std::string>();
        else if (j.contains("_id") && j["_id"].is_string()) b.id = j["_id"].get<std::string>();
        if (j.contains("owner") && j["owner"].is_string()) b.owner_id = j["owner"].get<std::string>();
        if (j.contains("name") && j["name"].is_string()) b.name = j["name"].get<std::string>();
        if (j.contains("description") && j["description"].is_string()) b.description = j["description"].get<std::string>();
        if (j.contains("token") && j["token"].is_string()) b.token = j["token"].get<std::string>();
        if (j.contains("public") && j["public"].is_boolean()) b.public_bot = j["public"].get<bool>();
        return b;
    }
};

} // namespace stoatpp::models
