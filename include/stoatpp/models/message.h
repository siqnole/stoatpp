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
    return nlohmann::json{};
}

inline Message Message::from_json(const nlohmann::json& j) {
    Message m;
    m.raw = j;
    return m;
}

inline nlohmann::json Message::to_json() const {
    return raw;
}

} // namespace stoatpp::models
