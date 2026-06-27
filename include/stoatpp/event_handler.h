#pragma once
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>

#include "models/user.h"
#include "models/channel.h"
#include "models/server.h"
#include "models/member.h"
#include "models/emoji.h"
#include "models/role.h"

namespace stoatpp::events {

struct Ready {
    models::User                    user;
    std::vector<models::Server>     servers;
    std::vector<models::Channel>    channels;
    std::vector<models::Member>     members;
    std::vector<models::Emoji>      emojis;
};

struct Message {
    std::string id;
    std::string channel_id;
    std::string server_id;   // optional, empty for DMs
    models::User author;
    std::string content;
    std::string nonce;
    bool edited = false;
    nlohmann::json raw;      // full raw JSON always available
};

struct MessageUpdate {
    std::string id;
    std::string channel_id;
    nlohmann::json data;     // partial Message object
};

struct MessageDelete {
    std::string id;
    std::string channel_id;
};

struct MessageReact {
    std::string id;           // message_id
    std::string channel_id;
    std::string user_id;
    std::string emoji_id;
};

struct MessageUnreact {
    std::string id;
    std::string channel_id;
    std::string user_id;
    std::string emoji_id;
};

struct ChannelCreate  { models::Channel channel; nlohmann::json raw; };
struct ChannelUpdate  { std::string id; nlohmann::json data; std::vector<std::string> clear; };
struct ChannelDelete  { std::string id; };

struct ServerCreate   { models::Server server; nlohmann::json raw; };
struct ServerUpdate   { std::string id; nlohmann::json data; std::vector<std::string> clear; };
struct ServerDelete   { std::string id; };

struct ServerMemberJoin  { std::string server_id; std::string user_id; models::Member member; };
struct ServerMemberLeave { std::string server_id; std::string user_id; };
struct ServerMemberUpdate {
    std::string server_id;
    std::string user_id;
    nlohmann::json data;
    std::vector<std::string> clear;
};

struct UserUpdate {
    std::string id;
    nlohmann::json data;
    std::vector<std::string> clear;
};

struct ChannelStartTyping { std::string channel_id; std::string user_id; };
struct ChannelStopTyping  { std::string channel_id; std::string user_id; };

struct Error   { std::string error_id; };
struct Logout  {};

} // namespace stoatpp::events

namespace stoatpp {

class event_dispatcher {
public:
    void on_ready(std::function<void(const events::Ready&)> cb);
    void on_message(std::function<void(const events::Message&)> cb);
    void on_message_update(std::function<void(const events::MessageUpdate&)> cb);
    void on_message_delete(std::function<void(const events::MessageDelete&)> cb);
    void on_message_react(std::function<void(const events::MessageReact&)> cb);
    void on_message_unreact(std::function<void(const events::MessageUnreact&)> cb);
    void on_channel_create(std::function<void(const events::ChannelCreate&)> cb);
    void on_channel_update(std::function<void(const events::ChannelUpdate&)> cb);
    void on_channel_delete(std::function<void(const events::ChannelDelete&)> cb);
    void on_server_create(std::function<void(const events::ServerCreate&)> cb);
    void on_server_update(std::function<void(const events::ServerUpdate&)> cb);
    void on_server_delete(std::function<void(const events::ServerDelete&)> cb);
    void on_server_member_join(std::function<void(const events::ServerMemberJoin&)> cb);
    void on_server_member_leave(std::function<void(const events::ServerMemberLeave&)> cb);
    void on_server_member_update(std::function<void(const events::ServerMemberUpdate&)> cb);
    void on_user_update(std::function<void(const events::UserUpdate&)> cb);
    void on_channel_start_typing(std::function<void(const events::ChannelStartTyping&)> cb);
    void on_channel_stop_typing(std::function<void(const events::ChannelStopTyping&)> cb);
    void on_error(std::function<void(const events::Error&)> cb);
    void on_logout(std::function<void(const events::Logout&)> cb);
    void on_raw_event(std::function<void(const std::string&, const nlohmann::json&)> cb);

    void dispatch_ready(const events::Ready& e);
    void dispatch_message(const events::Message& e);
    void dispatch_message_update(const events::MessageUpdate& e);
    void dispatch_message_delete(const events::MessageDelete& e);
    void dispatch_message_react(const events::MessageReact& e);
    void dispatch_message_unreact(const events::MessageUnreact& e);
    void dispatch_channel_create(const events::ChannelCreate& e);
    void dispatch_channel_update(const events::ChannelUpdate& e);
    void dispatch_channel_delete(const events::ChannelDelete& e);
    void dispatch_server_create(const events::ServerCreate& e);
    void dispatch_server_update(const events::ServerUpdate& e);
    void dispatch_server_delete(const events::ServerDelete& e);
    void dispatch_server_member_join(const events::ServerMemberJoin& e);
    void dispatch_server_member_leave(const events::ServerMemberLeave& e);
    void dispatch_server_member_update(const events::ServerMemberUpdate& e);
    void dispatch_user_update(const events::UserUpdate& e);
    void dispatch_channel_start_typing(const events::ChannelStartTyping& e);
    void dispatch_channel_stop_typing(const events::ChannelStopTyping& e);
    void dispatch_error(const events::Error& e);
    void dispatch_logout(const events::Logout& e);
    void dispatch_raw_event(const std::string& type, const nlohmann::json& data);

private:
    std::vector<std::function<void(const events::Ready&)>> ready_handlers_;
    std::vector<std::function<void(const events::Message&)>> message_handlers_;
    std::vector<std::function<void(const events::MessageUpdate&)>> message_update_handlers_;
    std::vector<std::function<void(const events::MessageDelete&)>> message_delete_handlers_;
    std::vector<std::function<void(const events::MessageReact&)>> message_react_handlers_;
    std::vector<std::function<void(const events::MessageUnreact&)>> message_unreact_handlers_;
    std::vector<std::function<void(const events::ChannelCreate&)>> channel_create_handlers_;
    std::vector<std::function<void(const events::ChannelUpdate&)>> channel_update_handlers_;
    std::vector<std::function<void(const events::ChannelDelete&)>> channel_delete_handlers_;
    std::vector<std::function<void(const events::ServerCreate&)>> server_create_handlers_;
    std::vector<std::function<void(const events::ServerUpdate&)>> server_update_handlers_;
    std::vector<std::function<void(const events::ServerDelete&)>> server_delete_handlers_;
    std::vector<std::function<void(const events::ServerMemberJoin&)>> server_member_join_handlers_;
    std::vector<std::function<void(const events::ServerMemberLeave&)>> server_member_leave_handlers_;
    std::vector<std::function<void(const events::ServerMemberUpdate&)>> server_member_update_handlers_;
    std::vector<std::function<void(const events::UserUpdate&)>> user_update_handlers_;
    std::vector<std::function<void(const events::ChannelStartTyping&)>> channel_start_typing_handlers_;
    std::vector<std::function<void(const events::ChannelStopTyping&)>> channel_stop_typing_handlers_;
    std::vector<std::function<void(const events::Error&)>> error_handlers_;
    std::vector<std::function<void(const events::Logout&)>> logout_handlers_;
    std::vector<std::function<void(const std::string&, const nlohmann::json&)>> raw_event_handlers_;

    mutable std::mutex mutex_;
};

} // namespace stoatpp
