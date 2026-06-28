#pragma once
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

// ---------------------------------------------------------------------------
// Embed builder
// Revolt sendable embed. Construct one, fill fields, then call .to_json()
// and push the result into MessagePayload::embeds.
// To show an image: call set_image(file_id) where file_id is an Autumn ID
// obtained via bot.rest().upload_from_url(url).
// ---------------------------------------------------------------------------
struct Embed {
    std::optional<std::string>   title;
    std::optional<std::string>   description;
    std::optional<std::string>   url;          // link on the title
    std::optional<std::string>   icon_url;     // small icon next to title
    std::optional<std::string>   colour;       // CSS colour string, e.g. "#5865f2"
    std::optional<std::string>   media;        // Autumn file ID for embed image

    // Set the embed image from an Autumn file ID.
    // Get the ID via: std::string id = bot.rest().upload_from_url(image_url);
    Embed& set_image(const std::string& autumn_file_id) {
        media = autumn_file_id;
        return *this;
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        if (title)       j["title"]       = *title;
        if (description) j["description"] = *description;
        if (url)         j["url"]         = *url;
        if (icon_url)    j["icon_url"]    = *icon_url;
        if (colour)      j["colour"]      = *colour;
        if (media)       j["media"]       = *media;
        return j;
    }
};


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

    // library-only: auto-delete this message after N seconds (0 = disabled)
    int delete_after = 0;

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

struct MessageQuery {
    std::optional<int> limit;
    std::optional<std::string> before;
    std::optional<std::string> after;
    std::optional<std::string> sort; // "Latest" | "Oldest"
    std::optional<std::string> nearby;

    std::string to_query_string() const {
        std::string q = "";
        if (limit) q += (q.empty() ? "?" : "&") + std::string("limit=") + std::to_string(*limit);
        if (before) q += (q.empty() ? "?" : "&") + std::string("before=") + *before;
        if (after) q += (q.empty() ? "?" : "&") + std::string("after=") + *after;
        if (sort) q += (q.empty() ? "?" : "&") + std::string("sort=") + *sort;
        if (nearby) q += (q.empty() ? "?" : "&") + std::string("nearby=") + *nearby;
        return q;
    }
};

struct MessageSearchQuery {
    std::optional<std::string> query;
    std::optional<int> limit;
    std::optional<std::string> before;
    std::optional<std::string> after;
    std::optional<std::string> sort; // "Latest" | "Oldest" | "Relevance"
    std::optional<bool> pinned;
    std::optional<bool> include_users;

    nlohmann::json to_json() const {
        nlohmann::json j = nlohmann::json::object();
        if (query) j["query"] = *query;
        if (limit) j["limit"] = *limit;
        if (before) j["before"] = *before;
        if (after) j["after"] = *after;
        if (sort) j["sort"] = *sort;
        if (pinned) j["pinned"] = *pinned;
        if (include_users) j["include_users"] = *include_users;
        return j;
    }
};

} // namespace stoatpp::models
