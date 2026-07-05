#pragma once
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>
#include "client_config.h"
#include "utils/ratelimiter.h"
#include "models/system.h"
#include "models/webhook.h"
#include "models/attachment.h"
#include "models/user_settings.h"
#include "models/invite.h"
#include "models/ban.h"
#include "models/bot.h"
#include "models/message.h"

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
    Response del(const std::string& path, const nlohmann::json& body = {});

    Response upload_file(const std::string& path,
                         const std::string& filename,
                         const std::vector<uint8_t>& data,
                         const std::string& mime_type);

    // Download an image from a public URL and upload it to Autumn.
    // Returns the Autumn file ID on success (empty string on failure).
    // Use the returned ID as Embed::media for embed images.
    std::string upload_from_url(const std::string& image_url,
                                const std::string& filename = "image.png",
                                const std::string& mime_type = "image/png");

    int remaining_calls(const std::string& bucket) const;
    int reset_after_ms(const std::string& bucket) const;
    void update_config(const ClientConfig& config) { config_ = config; }

    void set_pre_request_hook(std::function<void(const std::string& method,
                                                 const std::string& path,
                                                 nlohmann::json& body)> hook);

    // --- REST Helper Methods ---

    // 1. System & Utility Endpoints
    Response get_node_info();
    Response get_stats();
    Response get_gateway();
    Response create_report(const models::ReportPayload& payload);

    Response create_webhook(const std::string& channel_id, const std::string& name, const std::optional<std::string>& avatar_id = {});
    Response get_channel_webhooks(const std::string& channel_id);
    Response get_webhook(const std::string& webhook_id);
    Response edit_webhook(const std::string& webhook_id, const nlohmann::json& fields);
    Response delete_webhook(const std::string& webhook_id);
    Response execute_webhook(const std::string& webhook_id, const std::string& webhook_token, const models::WebhookExecutePayload& payload);
    Response execute_webhook_with_token(const std::string& webhook_id, const std::string& token, const models::WebhookExecutePayload& payload);
    Response get_webhook_with_token(const std::string& webhook_id, const std::string& token);
    Response edit_webhook_with_token(const std::string& webhook_id, const std::string& token, const nlohmann::json& fields);
    Response delete_webhook_with_token(const std::string& webhook_id, const std::string& token);
    Response edit_webhook_message(const std::string& webhook_id, const std::string& token, const std::string& message_id, const nlohmann::json& fields);
    Response delete_webhook_message(const std::string& webhook_id, const std::string& token, const std::string& message_id);
    Response execute_github_webhook(const std::string& webhook_id, const std::string& token, const nlohmann::json& payload);

    Response get_attachment_metadata(const std::string& attachment_id);

    Response get_user_settings();
    Response update_user_settings(const nlohmann::json& delta);
    Response get_unread_channels();

    // 2. Messages, Reactions & Pins
    Response create_message(const std::string& channel_id, const models::MessagePayload& payload);
    Response create_message(const std::string& channel_id, const nlohmann::json& payload_json);
    Response edit_message(const std::string& channel_id, const std::string& message_id, const nlohmann::json& fields);
    Response delete_message(const std::string& channel_id, const std::string& message_id);
    Response get_messages(const std::string& channel_id, const models::MessageQuery& query);
    Response get_message(const std::string& channel_id, const std::string& message_id);
    Response search_messages(const std::string& channel_id, const models::MessageSearchQuery& query);
    Response delete_messages_bulk(const std::string& channel_id, const std::vector<std::string>& message_ids);
    Response add_reaction(const std::string& channel_id, const std::string& message_id, const std::string& emoji);
    Response remove_reaction(const std::string& channel_id, const std::string& message_id, const std::string& emoji, const std::optional<std::string>& user_id = {});
    Response get_pinned_messages(const std::string& channel_id);
    Response pin_message(const std::string& channel_id, const std::string& message_id);
    Response unpin_message(const std::string& channel_id, const std::string& message_id);
    Response clear_reactions(const std::string& channel_id, const std::string& message_id);
    Response acknowledge_message(const std::string& channel_id, const std::string& message_id);
    Response acknowledge_channel(const std::string& channel_id);

    // 3. Servers, Members & Roles
    Response create_server(const std::string& name, const std::optional<std::string>& description = {});
    Response edit_server(const std::string& server_id, const nlohmann::json& fields);
    Response move_channel_to_category(const std::string& server_id, const std::string& channel_id, const std::string& category_id_or_name);
    Response leave_server(const std::string& server_id);
    Response get_server_invites(const std::string& server_id);
    Response get_server_bans(const std::string& server_id);
    Response ban_user(const std::string& server_id, const std::string& user_id, const std::optional<std::string>& reason = {});
    Response unban_user(const std::string& server_id, const std::string& user_id);
    Response get_server_members(const std::string& server_id);
    Response get_server(const std::string& server_id);
    Response get_server_member(const std::string& server_id, const std::string& user_id);
    Response get_server_emojis(const std::string& server_id);
    Response edit_member(const std::string& server_id, const std::string& user_id, const nlohmann::json& fields);
    Response kick_member(const std::string& server_id, const std::string& user_id);
    Response create_role(const std::string& server_id, const std::string& name);
    Response edit_role(const std::string& server_id, const std::string& role_id, const nlohmann::json& fields);
    Response delete_role(const std::string& server_id, const std::string& role_id);
    Response edit_role_ranks(const std::string& server_id, const std::vector<std::string>& rank_order);

    // 4. Channels & Permissions
    Response get_channel(const std::string& channel_id);
    Response get_server_channels(const std::string& server_id);
    Response create_channel(const std::string& server_id, const std::string& channel_type, const std::string& name);
    Response edit_channel(const std::string& channel_id, const nlohmann::json& fields);
    Response delete_channel(const std::string& channel_id);
    Response get_channel_invites(const std::string& channel_id);
    Response create_channel_invite(const std::string& channel_id);
    Response get_channel_permissions(const std::string& channel_id);
    Response set_channel_permission(const std::string& channel_id, const std::string& role_id, int64_t allow_mask, int64_t deny_mask);
    Response delete_channel_permission(const std::string& channel_id, const std::string& role_id);
    Response set_channel_default_permission(const std::string& channel_id, int64_t permissions);
    Response add_group_recipient(const std::string& channel_id, const std::string& user_id);
    Response remove_group_recipient(const std::string& channel_id, const std::string& user_id);

    // 5. Users & Relationships
    Response get_user(const std::string& user_id);
    Response get_user_profile(const std::string& user_id);
    Response search_users(const std::string& query);
    Response get_user_default_avatar(const std::string& user_id);
    Response get_user_flags(const std::string& user_id);
    Response change_username(const std::string& username, const std::optional<std::string>& password = {});
    Response get_mutual_friends_and_servers(const std::string& user_id);
    Response edit_current_user(const nlohmann::json& fields);
    Response get_relationships();
    Response set_relationship(const std::string& user_id, const std::string& relationship_status);
    Response delete_relationship(const std::string& user_id);

    // 6. Custom Emojis
    Response create_custom_emoji(const std::string& name, const std::string& file_id, const std::optional<std::string>& server_id = {});
    Response get_custom_emoji(const std::string& emoji_id);
    Response delete_custom_emoji(const std::string& emoji_id);

    // 7. Authentication & Session Management
    Response login_session(const std::string& email, const std::string& password, const std::string& friendly_name);
    Response logout_session();
    Response get_all_sessions();
    Response delete_session(const std::string& session_id);
    Response create_account(const std::string& email, const std::string& password);
    Response verify_account(const std::string& verification_token);
    Response reset_password(const std::string& email);
    Response reset_password_apply(const std::string& token, const std::string& new_password);
    Response change_password(const std::string& current_password, const std::string& new_password);
    Response change_email(const std::string& current_password, const std::string& new_email);
    Response delete_account();
    Response verify_totp(const std::string& mfa_token, const std::string& challenge_code);
    Response use_mfa_ticket(const std::string& ticket_id, const std::string& mfa_token);
    Response delete_all_sessions(bool revoke_self = false);
    Response get_mfa_methods();
    Response get_mfa_recovery_codes();
    Response regenerate_mfa_recovery_codes();

    // 8. Direct Messaging & Voice Calls
    Response get_active_dms();
    Response open_dm(const std::string& user_id);
    Response get_voice_call_info(const std::string& channel_id);
    Response create_group(const std::string& name, const std::vector<std::string>& users = {});
    Response get_group_members(const std::string& channel_id);
    Response join_voice_call(const std::string& channel_id);
    Response end_voice_ring(const std::string& channel_id, const std::string& user_id);

    // 9. Custom Bot Management
    Response create_bot(const std::string& name);
    Response get_bot(const std::string& bot_id);
    Response edit_bot(const std::string& bot_id, const nlohmann::json& fields);
    Response delete_bot(const std::string& bot_id);
    Response invite_bot(const std::string& bot_id, const std::string& server_id, const std::string& channel_id);
    Response get_owned_bots();
    Response get_bot_invite_info(const std::string& bot_id);

    // 10. Invites & Onboarding
    Response get_invite(const std::string& invite_code);
    Response accept_invite(const std::string& invite_code);
    Response get_onboarding_status();
    Response complete_onboarding(const std::string& username);

    void set_error_callback(std::function<void(const std::string& method, const std::string& path, int status_code, const std::string& error_msg)> cb) {
        error_callback_ = cb;
    }

private:
    std::string token_;
    ClientConfig config_;
    utils::ratelimiter ratelimiter_;
    std::function<void(const std::string& method, const std::string& path, nlohmann::json& body)> pre_request_hook_ = nullptr;
    std::function<void(const std::string& method, const std::string& path, int status_code, const std::string& error_msg)> error_callback_ = nullptr;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
};

} // namespace stoatpp
