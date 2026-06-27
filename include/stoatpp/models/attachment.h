#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct Attachment {
    std::string id;
    std::string tag;
    std::string filename;
    std::string content_type;
    int64_t size = 0;
    nlohmann::json metadata;

    static Attachment from_json(const nlohmann::json& j) {
        Attachment a;
        if (j.contains("id") && j["id"].is_string()) a.id = j["id"].get<std::string>();
        else if (j.contains("_id") && j["_id"].is_string()) a.id = j["_id"].get<std::string>();
        if (j.contains("tag") && j["tag"].is_string()) a.tag = j["tag"].get<std::string>();
        if (j.contains("filename") && j["filename"].is_string()) a.filename = j["filename"].get<std::string>();
        if (j.contains("content_type") && j["content_type"].is_string()) a.content_type = j["content_type"].get<std::string>();
        a.size = j.value("size", int64_t(0));
        if (j.contains("metadata")) a.metadata = j["metadata"];
        return a;
    }
};

} // namespace stoatpp::models
