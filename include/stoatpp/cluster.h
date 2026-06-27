#pragma once
#include <string>
#include <atomic>
#include <optional>
#include <vector>
#include <functional>
#include <unordered_map>
#include <shared_mutex>

#include "client_config.h"
#include "rest.h"
#include "gateway.h"
#include "event_handler.h"
#include "models/user.h"
#include "models/channel.h"
#include "models/server.h"
#include "models/message.h"

namespace stoatpp {

class bot_module;

class cluster {
public:
    explicit cluster(const std::string& token,
                     ClientConfig config = ClientConfig{});

    void start(bool return_after_init = false);
    void stop();

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
    void on_raw_event(std::function<void(const std::string& type, const nlohmann::json& data)> cb);

    void send_message(const std::string& channel_id,
                      const std::string& content,
                      std::function<void(models::Message, bool success)> callback = nullptr);

    void send_message(const std::string& channel_id,
                      const models::MessagePayload& payload,
                      std::function<void(models::Message, bool)> callback = nullptr);

    void edit_message(const std::string& channel_id,
                      const std::string& message_id,
                      const std::string& new_content,
                      std::function<void(models::Message, bool)> callback = nullptr);

    void delete_message(const std::string& channel_id,
                        const std::string& message_id,
                        std::function<void(bool)> callback = nullptr);

    void react_to_message(const std::string& channel_id,
                          const std::string& message_id,
                          const std::string& emoji_id,
                          std::function<void(bool)> callback = nullptr);

    void begin_typing(const std::string& channel_id);
    void end_typing(const std::string& channel_id);

    rest_client& rest();
    gateway& ws();

    std::optional<models::Server>  get_server(const std::string& id) const;
    std::optional<models::Channel> get_channel(const std::string& id) const;
    std::optional<models::User>    get_user(const std::string& id) const;

    models::User current_user() const;
    const ClientConfig& config() const;

    // Cog modules and Commands APIs
    void use(std::unique_ptr<bot_module> module);
    void register_command(const std::string& name,
                          std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb);

private:
    std::string      token_;
    ClientConfig     config_;
    rest_client      rest_;
    event_dispatcher dispatcher_;
    gateway          gateway_;

    mutable std::shared_mutex cache_mutex_;
    std::unordered_map<std::string, models::Server> server_cache_;
    std::unordered_map<std::string, models::Channel> channel_cache_;
    std::unordered_map<std::string, models::User> user_cache_;
    models::User current_user_;

    // Cogs & Commands state
    std::vector<std::unique_ptr<bot_module>> modules_;
    std::unordered_map<std::string, std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)>> commands_;
    std::atomic<bool> running_{false};
};

} // namespace stoatpp
