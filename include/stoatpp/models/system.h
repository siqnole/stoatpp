#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct InstanceConfig {
    std::string stoat;
    std::string version;
    
    struct Features {
        struct Captcha {
            bool enabled = false;
            std::string sitekey;
        };
        Captcha captcha;
        bool email = false;
        
        struct Autumn {
            bool enabled = false;
            std::string url;
        };
        Autumn autumn;
        
        struct January {
            bool enabled = false;
            std::string url;
        };
        January january;
    };
    Features features;
    std::string ws;

    static InstanceConfig from_json(const nlohmann::json& j) {
        InstanceConfig cfg;
        if (j.contains("stoat") && j["stoat"].is_string()) cfg.stoat = j["stoat"].get<std::string>();
        if (j.contains("version") && j["version"].is_string()) cfg.version = j["version"].get<std::string>();
        if (j.contains("features") && j["features"].is_object()) {
            const auto& feat = j["features"];
            if (feat.contains("captcha") && feat["captcha"].is_object()) {
                cfg.features.captcha.enabled = feat["captcha"].value("enabled", false);
                cfg.features.captcha.sitekey = feat["captcha"].value("sitekey", "");
            }
            cfg.features.email = feat.value("email", false);
            if (feat.contains("autumn") && feat["autumn"].is_object()) {
                cfg.features.autumn.enabled = feat["autumn"].value("enabled", false);
                cfg.features.autumn.url = feat["autumn"].value("url", "");
            }
            if (feat.contains("january") && feat["january"].is_object()) {
                cfg.features.january.enabled = feat["january"].value("enabled", false);
                cfg.features.january.url = feat["january"].value("url", "");
            }
        }
        if (j.contains("ws") && j["ws"].is_string()) cfg.ws = j["ws"].get<std::string>();
        return cfg;
    }
};

struct InstanceStats {
    int64_t users_count = 0;
    int64_t servers_count = 0;
    int64_t channels_count = 0;
    int64_t messages_count = 0;

    static InstanceStats from_json(const nlohmann::json& j) {
        InstanceStats stats;
        stats.users_count = j.value("users_count", int64_t(0));
        stats.servers_count = j.value("servers_count", int64_t(0));
        stats.channels_count = j.value("channels_count", int64_t(0));
        stats.messages_count = j.value("messages_count", int64_t(0));
        return stats;
    }
};

struct ReportPayload {
    std::string content_type; // "Message", "Server", "User"
    std::string id;
    std::string reason;
    std::optional<std::string> additional_context;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["content_type"] = content_type;
        j["id"] = id;
        j["reason"] = reason;
        if (additional_context) j["additional_context"] = *additional_context;
        return j;
    }
};

} // namespace stoatpp::models
