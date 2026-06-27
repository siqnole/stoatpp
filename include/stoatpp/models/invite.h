#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Invite {
    std::string code;
    std::string channel_id;
    std::optional<std::string> server_id;
    std::string creator_id;

    static Invite from_json(const nlohmann::json& j) {
        Invite inv;
        if (j.contains("code") && j["code"].is_string()) inv.code = j["code"].get<std::string>();
        else if (j.contains("_id") && j["_id"].is_string()) inv.code = j["_id"].get<std::string>();
        if (j.contains("channel") && j["channel"].is_string()) inv.channel_id = j["channel"].get<std::string>();
        if (j.contains("server") && j["server"].is_string()) inv.server_id = j["server"].get<std::string>();
        if (j.contains("creator") && j["creator"].is_string()) inv.creator_id = j["creator"].get<std::string>();
        return inv;
    }
};

} // namespace stoatpp::models
