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

// 1. System & Utility Endpoints
rest_client::Response rest_client::get_node_info() {
    return get("/");
}

rest_client::Response rest_client::get_stats() {
    return get("/stats");
}

rest_client::Response rest_client::get_gateway() {
    return get("/gateway");
}

rest_client::Response rest_client::create_report(const models::ReportPayload& payload) {
    return post("/safety/report", payload.to_json());
}

rest_client::Response rest_client::create_webhook(const std::string& channel_id, const std::string& name, const std::optional<std::string>& avatar_id) {
    nlohmann::json body;
    body["name"] = name;
    if (avatar_id) body["avatar"] = *avatar_id;
    return post("/channels/" + channel_id + "/webhooks", body);
}

rest_client::Response rest_client::get_channel_webhooks(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/webhooks");
}

rest_client::Response rest_client::delete_webhook(const std::string& webhook_id) {
    return del("/webhooks/" + webhook_id);
}

rest_client::Response rest_client::execute_webhook(const std::string& webhook_id, const std::string& webhook_token, const models::WebhookExecutePayload& payload) {
    return post("/webhooks/" + webhook_id + "/" + webhook_token, payload.to_json());
}

rest_client::Response rest_client::get_attachment_metadata(const std::string& attachment_id) {
    return get("/attachments/" + attachment_id);
}

rest_client::Response rest_client::get_user_settings() {
    return get("/sync/settings");
}

rest_client::Response rest_client::update_user_settings(const nlohmann::json& delta) {
    return post("/sync/settings", delta);
}

rest_client::Response rest_client::get_unread_channels() {
    return get("/sync/unreads");
}

// 2. Messages, Reactions & Pins
rest_client::Response rest_client::get_messages(const std::string& channel_id, const models::MessageQuery& query) {
    return get("/channels/" + channel_id + "/messages" + query.to_query_string());
}

rest_client::Response rest_client::get_message(const std::string& channel_id, const std::string& message_id) {
    return get("/channels/" + channel_id + "/messages/" + message_id);
}

rest_client::Response rest_client::delete_messages_bulk(const std::string& channel_id, const std::vector<std::string>& message_ids) {
    nlohmann::json body;
    body["ids"] = message_ids;
    return post("/channels/" + channel_id + "/messages/bulk", body);
}

rest_client::Response rest_client::remove_reaction(const std::string& channel_id, const std::string& message_id, const std::string& emoji, const std::optional<std::string>& user_id) {
    std::string path = "/channels/" + channel_id + "/messages/" + message_id + "/reactions/" + emoji;
    if (user_id) path += "?user_id=" + *user_id;
    return del(path);
}

rest_client::Response rest_client::get_pinned_messages(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/pins");
}

rest_client::Response rest_client::pin_message(const std::string& channel_id, const std::string& message_id) {
    return put("/channels/" + channel_id + "/pins/" + message_id);
}

rest_client::Response rest_client::unpin_message(const std::string& channel_id, const std::string& message_id) {
    return del("/channels/" + channel_id + "/pins/" + message_id);
}

// 3. Servers, Members & Roles
rest_client::Response rest_client::create_server(const std::string& name, const std::optional<std::string>& description) {
    nlohmann::json body;
    body["name"] = name;
    if (description) body["description"] = *description;
    return post("/servers", body);
}

rest_client::Response rest_client::edit_server(const std::string& server_id, const nlohmann::json& fields) {
    return patch("/servers/" + server_id, fields);
}

rest_client::Response rest_client::leave_server(const std::string& server_id) {
    return del("/servers/" + server_id);
}

rest_client::Response rest_client::get_server_invites(const std::string& server_id) {
    return get("/servers/" + server_id + "/invites");
}

rest_client::Response rest_client::get_server_bans(const std::string& server_id) {
    return get("/servers/" + server_id + "/bans");
}

rest_client::Response rest_client::ban_user(const std::string& server_id, const std::string& user_id, const std::optional<std::string>& reason) {
    nlohmann::json body = nlohmann::json::object();
    if (reason) body["reason"] = *reason;
    return put("/servers/" + server_id + "/bans/" + user_id, body);
}

rest_client::Response rest_client::unban_user(const std::string& server_id, const std::string& user_id) {
    return del("/servers/" + server_id + "/bans/" + user_id);
}

rest_client::Response rest_client::get_server_members(const std::string& server_id) {
    return get("/servers/" + server_id + "/members");
}

rest_client::Response rest_client::edit_member(const std::string& server_id, const std::string& user_id, const nlohmann::json& fields) {
    return patch("/servers/" + server_id + "/members/" + user_id, fields);
}

rest_client::Response rest_client::kick_member(const std::string& server_id, const std::string& user_id) {
    return del("/servers/" + server_id + "/members/" + user_id);
}

rest_client::Response rest_client::create_role(const std::string& server_id, const std::string& name) {
    nlohmann::json body;
    body["name"] = name;
    return post("/servers/" + server_id + "/roles", body);
}

rest_client::Response rest_client::edit_role(const std::string& server_id, const std::string& role_id, const nlohmann::json& fields) {
    return patch("/servers/" + server_id + "/roles/" + role_id, fields);
}

rest_client::Response rest_client::delete_role(const std::string& server_id, const std::string& role_id) {
    return del("/servers/" + server_id + "/roles/" + role_id);
}

// 4. Channels & Permissions
rest_client::Response rest_client::get_server_channels(const std::string& server_id) {
    return get("/servers/" + server_id + "/channels");
}

rest_client::Response rest_client::create_channel(const std::string& server_id, const std::string& channel_type, const std::string& name) {
    nlohmann::json body;
    body["channel_type"] = channel_type;
    body["name"] = name;
    return post("/servers/" + server_id + "/channels", body);
}

rest_client::Response rest_client::edit_channel(const std::string& channel_id, const nlohmann::json& fields) {
    return patch("/channels/" + channel_id, fields);
}

rest_client::Response rest_client::delete_channel(const std::string& channel_id) {
    return del("/channels/" + channel_id);
}

rest_client::Response rest_client::get_channel_invites(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/invites");
}

rest_client::Response rest_client::create_channel_invite(const std::string& channel_id) {
    return post("/channels/" + channel_id + "/invites");
}

rest_client::Response rest_client::get_channel_permissions(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/permissions");
}

rest_client::Response rest_client::set_channel_permission(const std::string& channel_id, const std::string& role_id, int64_t allow_mask, int64_t deny_mask) {
    nlohmann::json body;
    body["permissions"] = {{"allow", allow_mask}, {"deny", deny_mask}};
    return put("/channels/" + channel_id + "/permissions/" + role_id, body);
}

rest_client::Response rest_client::delete_channel_permission(const std::string& channel_id, const std::string& role_id) {
    return del("/channels/" + channel_id + "/permissions/" + role_id);
}

rest_client::Response rest_client::add_group_recipient(const std::string& channel_id, const std::string& user_id) {
    return put("/channels/" + channel_id + "/recipients/" + user_id);
}

rest_client::Response rest_client::remove_group_recipient(const std::string& channel_id, const std::string& user_id) {
    return del("/channels/" + channel_id + "/recipients/" + user_id);
}

// 5. Users & Relationships
rest_client::Response rest_client::get_user_profile(const std::string& user_id) {
    return get("/users/" + user_id + "/profile");
}

rest_client::Response rest_client::get_mutual_friends_and_servers(const std::string& user_id) {
    return get("/users/" + user_id + "/mutual");
}

rest_client::Response rest_client::edit_current_user(const nlohmann::json& fields) {
    return patch("/users/@me", fields);
}

rest_client::Response rest_client::get_relationships() {
    return get("/users/relationships");
}

rest_client::Response rest_client::set_relationship(const std::string& user_id, const std::string& relationship_status) {
    nlohmann::json body;
    body["status"] = relationship_status;
    return put("/users/" + user_id + "/relationship", body);
}

rest_client::Response rest_client::delete_relationship(const std::string& user_id) {
    return del("/users/" + user_id + "/relationship");
}

// 6. Custom Emojis
rest_client::Response rest_client::create_custom_emoji(const std::string& name, const std::string& file_id, const std::optional<std::string>& server_id) {
    nlohmann::json body;
    body["name"] = name;
    body["file_id"] = file_id;
    if (server_id) body["server_id"] = *server_id;
    return post("/custom/emoji", body);
}

rest_client::Response rest_client::get_custom_emoji(const std::string& emoji_id) {
    return get("/custom/emoji/" + emoji_id);
}

rest_client::Response rest_client::delete_custom_emoji(const std::string& emoji_id) {
    return del("/custom/emoji/" + emoji_id);
}

// 7. Authentication & Session Management
rest_client::Response rest_client::login_session(const std::string& email, const std::string& password, const std::string& friendly_name) {
    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    body["friendly_name"] = friendly_name;
    return post("/auth/session/login", body);
}

rest_client::Response rest_client::logout_session() {
    return post("/auth/session/logout");
}

rest_client::Response rest_client::get_all_sessions() {
    return get("/auth/session/all");
}

rest_client::Response rest_client::delete_session(const std::string& session_id) {
    return del("/auth/session/" + session_id);
}

rest_client::Response rest_client::create_account(const std::string& email, const std::string& password) {
    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    return post("/auth/account/create", body);
}

rest_client::Response rest_client::verify_account(const std::string& verification_token) {
    nlohmann::json body;
    body["token"] = verification_token;
    return post("/auth/account/verify", body);
}

rest_client::Response rest_client::reset_password(const std::string& email) {
    nlohmann::json body;
    body["email"] = email;
    return post("/auth/account/password/reset", body);
}

rest_client::Response rest_client::delete_account() {
    return post("/auth/account/delete");
}

rest_client::Response rest_client::verify_totp(const std::string& mfa_token, const std::string& challenge_code) {
    nlohmann::json body;
    body["token"] = mfa_token;
    body["code"] = challenge_code;
    return post("/auth/mfa/totp", body);
}

rest_client::Response rest_client::use_mfa_ticket(const std::string& ticket_id, const std::string& mfa_token) {
    nlohmann::json body;
    body["ticket"] = ticket_id;
    body["token"] = mfa_token;
    return post("/auth/mfa/ticket", body);
}

// 8. Direct Messaging & Voice Calls
rest_client::Response rest_client::get_active_dms() {
    return get("/users/dms");
}

rest_client::Response rest_client::open_dm(const std::string& user_id) {
    return post("/users/" + user_id + "/dm");
}

rest_client::Response rest_client::get_voice_call_info(const std::string& channel_id) {
    return get("/channels/" + channel_id + "/call");
}

// 9. Custom Bot Management
rest_client::Response rest_client::create_bot(const std::string& name) {
    nlohmann::json body;
    body["name"] = name;
    return post("/bots", body);
}

rest_client::Response rest_client::get_bot(const std::string& bot_id) {
    return get("/bots/" + bot_id);
}

rest_client::Response rest_client::edit_bot(const std::string& bot_id, const nlohmann::json& fields) {
    return patch("/bots/" + bot_id, fields);
}

rest_client::Response rest_client::delete_bot(const std::string& bot_id) {
    return del("/bots/" + bot_id);
}

rest_client::Response rest_client::invite_bot(const std::string& bot_id, const std::string& server_id, const std::string& channel_id) {
    nlohmann::json body;
    body["server"] = server_id;
    body["channel"] = channel_id;
    return post("/bots/" + bot_id + "/invite", body);
}

} // namespace stoatpp
