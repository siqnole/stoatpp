#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include "client_config.h"
#include "utils/ratelimiter.h"

namespace stoatpp {

class rest_client {
public:
    rest_client(const std::string& token, const ClientConfig& config);

    struct Response {
        int         status_code;
        nlohmann::json body;
        bool        success() const { return status_code >= 200 && status_code < 300; }
        std::string error_message() const;
    };

    Response get(const std::string& path);
    Response post(const std::string& path, const nlohmann::json& body = {});
    Response patch(const std::string& path, const nlohmann::json& body = {});
    Response put(const std::string& path, const nlohmann::json& body = {});
    Response del(const std::string& path);

    Response upload_file(const std::string& path,
                         const std::string& filename,
                         const std::vector<uint8_t>& data,
                         const std::string& mime_type);

    int remaining_calls(const std::string& bucket) const;
    int reset_after_ms(const std::string& bucket) const;

    void set_pre_request_hook(std::function<void(const std::string& method,
                                                 const std::string& path,
                                                 nlohmann::json& body)> hook);

private:
    std::string token_;
    ClientConfig config_;
    utils::ratelimiter ratelimiter_;
    std::function<void(const std::string& method, const std::string& path, nlohmann::json& body)> pre_request_hook_ = nullptr;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
};

} // namespace stoatpp
