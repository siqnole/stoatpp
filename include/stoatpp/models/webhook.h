#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Webhook {
    std::string id;
    std::string name;
    std::string channel_id;
    std::optional<std::string> avatar;
    std::optional<std::string> token;
    nlohmann::json raw;

    static Webhook from_json(const nlohmann::json& j) {
        Webhook w;
        w.raw = j;
        if (j.contains("id") && j["id"].is_string()) w.id = j["id"].get<std::string>();
        else if (j.contains("_id") && j["_id"].is_string()) w.id = j["_id"].get<std::string>();
        if (j.contains("name") && j["name"].is_string()) w.name = j["name"].get<std::string>();
        if (j.contains("channel_id") && j["channel_id"].is_string()) w.channel_id = j["channel_id"].get<std::string>();
        if (j.contains("avatar") && j["avatar"].is_string()) w.avatar = j["avatar"].get<std::string>();
        if (j.contains("token") && j["token"].is_string()) w.token = j["token"].get<std::string>();
        return w;
    }

    nlohmann::json to_json() const {
        nlohmann::json j = raw;
        j["id"] = id;
        j["name"] = name;
        j["channel_id"] = channel_id;
        if (avatar) j["avatar"] = *avatar;
        if (token) j["token"] = *token;
        return j;
    }
};

struct WebhookExecutePayload {
    std::string content;
    std::optional<std::string> username;
    std::optional<std::string> avatar_url;
    std::vector<nlohmann::json> embeds;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["content"] = content;
        if (username) j["username"] = *username;
        if (avatar_url) j["avatar_url"] = *avatar_url;
        if (!embeds.empty()) j["embeds"] = embeds;
        return j;
    }
};

} // namespace stoatpp::models
