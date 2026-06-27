#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace stoatpp::models {

struct UserSettings {
    std::unordered_map<std::string, nlohmann::json> settings;

    static UserSettings from_json(const nlohmann::json& j) {
        UserSettings us;
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                us.settings[it.key()] = it.value();
            }
        }
        return us;
    }

    nlohmann::json to_json() const {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [k, v] : settings) {
            j[k] = v;
        }
        return j;
    }
};

} // namespace stoatpp::models
