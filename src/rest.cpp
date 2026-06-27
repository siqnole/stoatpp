#include "stoatpp/rest.h"
#include "stoatpp/exceptions.h"
#include "stoatpp/utils/logger.h"
#include <httplib.h>
#include <chrono>
#include <thread>
#include <iostream>

namespace stoatpp {

rest_client::rest_client(const std::string& token, const ClientConfig& config)
    : token_(token), config_(config) {}

std::string rest_client::Response::error_message() const {
    if (body.is_object() && body.contains("error") && body["error"].is_string()) {
        return body["error"].get<std::string>();
    }
    return "HTTP error " + std::to_string(status_code);
}

static rest_client::Response perform_request(
    const std::string& method,
    const std::string& path,
    nlohmann::json body,
    const std::string& token,
    const ClientConfig& config,
    utils::ratelimiter& ratelimiter,
    std::function<void(const std::string&, const std::string&, nlohmann::json&)> pre_hook
) {
    if (pre_hook) {
        pre_hook(method, path, body);
    }

    std::string bucket = method + ":" + path;
    if (config.http_respect_ratelimits) {
        int wait_time = ratelimiter.check(bucket);
        if (wait_time > 0) {
            utils::logger::log(LogLevel::WARNING, "Rate limit active for " + bucket + ". Sleeping for " + std::to_string(wait_time) + "ms", config);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }
    }

    int retries = 0;
    while (true) {
        // Parse host from api_base_url (cpp-httplib works best when constructor takes the scheme+host)
        httplib::Client cli(config.api_base_url);
        cli.set_connection_timeout(std::chrono::milliseconds(config.http_timeout_ms));
        cli.set_read_timeout(std::chrono::milliseconds(config.http_timeout_ms));

        httplib::Headers headers;
        if (config.token_type == TokenType::BOT) {
            headers.emplace("X-Bot-Token", token);
        } else {
            headers.emplace("X-Session-Token", token);
        }

        for (const auto& [k, v] : config.extra_headers) {
            headers.emplace(k, v);
        }

        httplib::Result res;
        std::string req_body_str = body.is_null() ? "" : body.dump();
        
        utils::logger::log(LogLevel::DEBUG, "REST: sending " + method + " " + path, config);

        if (method == "GET") {
            res = cli.Get(path, headers);
        } else if (method == "POST") {
            res = cli.Post(path, headers, req_body_str, "application/json");
        } else if (method == "PATCH") {
            res = cli.Patch(path, headers, req_body_str, "application/json");
        } else if (method == "PUT") {
            res = cli.Put(path, headers, req_body_str, "application/json");
        } else if (method == "DELETE") {
            res = cli.Delete(path, headers);
        } else {
            throw StoatException("Unsupported HTTP method: " + method);
        }

        if (!res) {
            auto err = res.error();
            std::string err_msg = "Connection failed on " + method + " " + path + ": " + httplib::to_string(err);
            utils::logger::log(LogLevel::ERROR, err_msg, config);
            
            if (retries < config.http_retry_count) {
                retries++;
                utils::logger::log(LogLevel::INFO, "Retrying REST request (" + std::to_string(retries) + "/" + std::to_string(config.http_retry_count) + ")", config);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            throw NetworkError(err_msg);
        }

        int status = res->status;
        nlohmann::json resp_body;
        try {
            if (!res->body.empty()) {
                resp_body = nlohmann::json::parse(res->body);
            }
        } catch (...) {
            resp_body = nlohmann::json{{"raw_body", res->body}};
        }

        std::string bucket_id = res->get_header_value("X-RateLimit-Bucket");
        if (bucket_id.empty()) bucket_id = bucket;

        std::string limit_val = res->get_header_value("X-RateLimit-Limit");
        std::string remaining_val = res->get_header_value("X-RateLimit-Remaining");
        std::string reset_val = res->get_header_value("X-RateLimit-Reset-After");

        if (!limit_val.empty() && !remaining_val.empty() && !reset_val.empty()) {
            try {
                int limit = std::stoi(limit_val);
                int remaining = std::stoi(remaining_val);
                int reset_after = std::stoi(reset_val);
                
                utils::logger::log(LogLevel::TRACE, "RateLimit Headers for Bucket " + bucket_id + ": Limit=" + limit_val + " Remaining=" + remaining_val + " Reset=" + reset_val + "ms", config);
                ratelimiter.update(bucket_id, limit, remaining, reset_after);
            } catch (...) {}
        }

        if (status == 429) {
            int retry_after = config.ratelimit_retry_delay_ms;
            if (resp_body.is_object() && resp_body.contains("retry_after") && resp_body["retry_after"].is_number()) {
                retry_after = resp_body["retry_after"].get<int>();
            } else if (!reset_val.empty()) {
                try { retry_after = std::stoi(reset_val); } catch (...) {}
            }

            utils::logger::log(LogLevel::WARNING, "Rate limited (429). Retrying in " + std::to_string(retry_after) + "ms", config);
            
            if (config.http_respect_ratelimits) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_after));
                continue;
            } else {
                throw RateLimitError("Rate limit hit on " + path, retry_after);
            }
        }

        return rest_client::Response{status, resp_body};
    }
}

rest_client::Response rest_client::get(const std::string& path) {
    return perform_request("GET", path, nullptr, token_, config_, ratelimiter_, pre_request_hook_);
}

rest_client::Response rest_client::post(const std::string& path, const nlohmann::json& body) {
    return perform_request("POST", path, body, token_, config_, ratelimiter_, pre_request_hook_);
}

rest_client::Response rest_client::patch(const std::string& path, const nlohmann::json& body) {
    return perform_request("PATCH", path, body, token_, config_, ratelimiter_, pre_request_hook_);
}

rest_client::Response rest_client::put(const std::string& path, const nlohmann::json& body) {
    return perform_request("PUT", path, body, token_, config_, ratelimiter_, pre_request_hook_);
}

rest_client::Response rest_client::del(const std::string& path) {
    return perform_request("DELETE", path, nullptr, token_, config_, ratelimiter_, pre_request_hook_);
}

rest_client::Response rest_client::upload_file(const std::string& path,
                                              const std::string& filename,
                                              const std::vector<uint8_t>& data,
                                              const std::string& mime_type) {
    httplib::Client cli(config_.api_base_url);
    cli.set_connection_timeout(std::chrono::milliseconds(config_.http_timeout_ms));
    cli.set_read_timeout(std::chrono::milliseconds(config_.http_timeout_ms));

    httplib::Headers headers;
    if (config_.token_type == TokenType::BOT) {
        headers.emplace("X-Bot-Token", token_);
    } else {
        headers.emplace("X-Session-Token", token_);
    }
    for (const auto& [k, v] : config_.extra_headers) {
        headers.emplace(k, v);
    }

    std::string file_content(data.begin(), data.end());
    httplib::MultipartFormDataItems items = {
        { "file", file_content, filename, mime_type }
    };

    utils::logger::log(LogLevel::DEBUG, "REST: sending file upload to " + path, config_);
    auto res = cli.Post(path, headers, items);
    if (!res) {
        throw NetworkError("File upload failed");
    }

    nlohmann::json resp_body;
    try {
        if (!res->body.empty()) {
            resp_body = nlohmann::json::parse(res->body);
        }
    } catch (...) {
        resp_body = nlohmann::json{{"raw_body", res->body}};
    }

    return Response{res->status, resp_body};
}

int rest_client::remaining_calls(const std::string& bucket) const {
    return 10;
}

int rest_client::reset_after_ms(const std::string& bucket) const {
    return ratelimiter_.check(bucket);
}

void rest_client::set_pre_request_hook(std::function<void(const std::string& method,
                                                           const std::string& path,
                                                           nlohmann::json& body)> hook) {
    pre_request_hook_ = hook;
}

} // namespace stoatpp
