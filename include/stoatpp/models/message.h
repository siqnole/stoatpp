#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct MessagePayload {
    std::string content;
    std::optional<std::string> nonce;

    struct Reply {
        std::string id;
        bool mention = false;
    };
    std::vector<Reply> replies;

    struct Masquerade {
        std::optional<std::string> name;
        std::optional<std::string> avatar;
        std::optional<std::string> colour;
    };
    std::optional<Masquerade> masquerade;

    // Embeds (send custom embed objects)
    std::vector<nlohmann::json> embeds;

    nlohmann::json to_json() const;
};

struct Message {
    std::string id;
    std::string channel;
    std::string author;                            // user ID
    std::optional<std::string> content;
    std::optional<std::string> edited_at;
    std::vector<std::string> mentions;
    std::vector<std::string> replies;
    nlohmann::json embeds;                         // raw for now
    nlohmann::json attachments;
    nlohmann::json raw;                            // original parsed object

    static Message from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

inline nlohmann::json MessagePayload::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    j["content"] = content;
    if (nonce) j["nonce"] = *nonce;
    
    if (!replies.empty()) {
        nlohmann::json replies_json = nlohmann::json::array();
        for (const auto& r : replies) {
            nlohmann::json rep = nlohmann::json::object();
            rep["id"] = r.id;
            rep["mention"] = r.mention;
            replies_json.push_back(rep);
        }
        j["replies"] = replies_json;
    }
    
    if (masquerade) {
        nlohmann::json masq = nlohmann::json::object();
        if (masquerade->name) masq["name"] = *(masquerade->name);
        if (masquerade->avatar) masq["avatar"] = *(masquerade->avatar);
        if (masquerade->colour) masq["colour"] = *(masquerade->colour);
        j["masquerade"] = masq;
    }
    
    if (!embeds.empty()) {
        j["embeds"] = embeds;
    }
    
    return j;
}

inline Message Message::from_json(const nlohmann::json& j) {
    Message m;
    m.raw = j;
    if (j.contains("id") && j["id"].is_string()) m.id = j["id"].get<std::string>();
    else if (j.contains("_id") && j["_id"].is_string()) m.id = j["_id"].get<std::string>();
    if (j.contains("channel") && j["channel"].is_string()) m.channel = j["channel"].get<std::string>();
    
    if (j.contains("author") && j["author"].is_string()) m.author = j["author"].get<std::string>();
    
    if (j.contains("content") && j["content"].is_string()) m.content = j["content"].get<std::string>();
    if (j.contains("edited_at") && j["edited_at"].is_string()) m.edited_at = j["edited_at"].get<std::string>();
    
    if (j.contains("mentions") && j["mentions"].is_array()) {
        for (const auto& men : j["mentions"]) {
            if (men.is_string()) m.mentions.push_back(men.get<std::string>());
        }
    }
    
    if (j.contains("replies") && j["replies"].is_array()) {
        for (const auto& rep : j["replies"]) {
            if (rep.is_string()) m.replies.push_back(rep.get<std::string>());
        }
    }
    
    if (j.contains("embeds")) m.embeds = j["embeds"];
    else m.embeds = nlohmann::json::array();
    
    if (j.contains("attachments")) m.attachments = j["attachments"];
    else m.attachments = nlohmann::json::array();
    
    return m;
}

inline nlohmann::json Message::to_json() const {
    nlohmann::json j = raw;
    j["id"] = id;
    j["channel"] = channel;
    j["author"] = author;
    if (content) j["content"] = *content;
    if (edited_at) j["edited_at"] = *edited_at;
    j["mentions"] = mentions;
    j["replies"] = replies;
    j["embeds"] = embeds;
    j["attachments"] = attachments;
    return j;
}

} // namespace stoatpp::models
