#pragma once
#include <string>
#include <atomic>
#include <optional>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <shared_mutex>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "client_config.h"
#include "utils/timestamp.h"
#include "rest.h"
#include "gateway.h"
#include "event_handler.h"
#include "models/user.h"
#include "models/channel.h"
#include "models/server.h"
#include "models/message.h"
#include "models/permissions.h"

namespace stoatpp {

class bot_module;
class cluster;

struct timer {
    uint64_t id;
    uint64_t interval_seconds;
};

using timer_callback = std::function<void(timer)>;

struct Command {
    std::string name;
    std::vector<std::string> aliases;
    std::string description;
    std::string usage;
    std::vector<std::string> args;
    std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> callback;
    std::string category;
    int64_t required_permissions = 0;
    int64_t cooldown_seconds = 0;
    std::vector<std::string> subcommands;
};

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
    void on_server_role_create(std::function<void(const events::ServerRoleCreate&)> cb);
    void on_server_role_update(std::function<void(const events::ServerRoleUpdate&)> cb);
    void on_server_role_delete(std::function<void(const events::ServerRoleDelete&)> cb);
    void on_user_update(std::function<void(const events::UserUpdate&)> cb);
    void on_user_settings_update(std::function<void(const events::UserSettingsUpdate&)> cb);
    void on_user_presence_update(std::function<void(const events::UserPresenceUpdate&)> cb);
    void on_channel_start_typing(std::function<void(const events::ChannelStartTyping&)> cb);
    void on_channel_stop_typing(std::function<void(const events::ChannelStopTyping&)> cb);
    void on_webhook_update(std::function<void(const events::WebhookUpdate&)> cb);
    void on_voice_state_update(std::function<void(const events::VoiceStateUpdate&)> cb);
    void on_error(std::function<void(const events::Error&)> cb);
    void on_logout(std::function<void(const events::Logout&)> cb);
    void on_raw_event(std::function<void(const std::string& type, const nlohmann::json& data)> cb);

    void send_message(const std::string& channel_id,
                      const std::string& content,
                      std::function<void(models::Message, bool success)> callback = nullptr);

    void send_message(const std::string& channel_id,
                      const models::MessagePayload& payload,
                      std::function<void(models::Message, bool)> callback = nullptr);

    void send_reply(const std::string& channel_id,
                    const std::string& message_id,
                    const std::string& content,
                    bool mention = true,
                    std::function<void(models::Message, bool success)> callback = nullptr);

    void send_reply(const events::Message& to_message,
                    const std::string& content,
                    bool mention = true,
                    std::function<void(models::Message, bool success)> callback = nullptr);

    void edit_message(const std::string& channel_id,
                      const std::string& message_id,
                      const std::string& new_content,
                      std::function<void(models::Message, bool)> callback = nullptr);

    void edit_message(const std::string& channel_id,
                      const std::string& message_id,
                      const models::MessagePayload& payload,
                      std::function<void(models::Message, bool)> callback = nullptr);

    void delete_message(const std::string& channel_id,
                        const std::string& message_id,
                        std::function<void(bool)> callback = nullptr);

    void pin_message(const std::string& channel_id,
                     const std::string& message_id,
                     std::function<void(bool)> callback = nullptr);

    void unpin_message(const std::string& channel_id,
                       const std::string& message_id,
                       std::function<void(bool)> callback = nullptr);

    void react_to_message(const std::string& channel_id,
                          const std::string& message_id,
                          const std::string& emoji_id,
                          std::function<void(bool)> callback = nullptr);

    void unreact_from_message(const std::string& channel_id,
                              const std::string& message_id,
                              const std::string& emoji_id,
                              const std::optional<std::string>& user_id = {},
                              std::function<void(bool)> callback = nullptr);

    void begin_typing(const std::string& channel_id);
    void end_typing(const std::string& channel_id);

    void ban_user(const std::string& server_id,
                  const std::string& user_id,
                  const std::string& reason = "",
                  std::function<void(bool)> callback = nullptr);

    void unban_user(const std::string& server_id,
                    const std::string& user_id,
                    std::function<void(bool)> callback = nullptr);

    void kick_member(const std::string& server_id,
                     const std::string& user_id,
                     std::function<void(bool)> callback = nullptr);

    void timeout_member(const std::string& server_id,
                        const std::string& user_id,
                        const std::string& duration_iso,
                        std::function<void(bool)> callback = nullptr);

    void remove_timeout(const std::string& server_id,
                        const std::string& user_id,
                        std::function<void(bool)> callback = nullptr);

    void create_role(const std::string& server_id,
                     const std::string& name,
                     std::function<void(models::Role, bool)> callback = nullptr);

    void delete_role(const std::string& server_id,
                     const std::string& role_id,
                     std::function<void(bool)> callback = nullptr);

    void add_role_to_member(const std::string& server_id,
                            const std::string& user_id,
                            const std::string& role_id,
                            std::function<void(bool, std::string)> callback);

    void add_role_to_member(const std::string& server_id,
                            const std::string& user_id,
                            const std::string& role_id,
                            std::function<void(bool)> callback = nullptr);

    void remove_role_from_member(const std::string& server_id,
                                 const std::string& user_id,
                                 const std::string& role_id,
                                 std::function<void(bool, std::string)> callback);

    void remove_role_from_member(const std::string& server_id,
                                 const std::string& user_id,
                                 const std::string& role_id,
                                 std::function<void(bool)> callback = nullptr);

    void get_member_count(const std::string& server_id,
                          std::function<void(int count, bool success)> callback);

    void fetch_server_invites(const std::string& server_id,
                              std::function<void(std::vector<models::Invite> invites, bool success)> callback);

    void create_invite(const std::string& channel_id,
                       std::function<void(models::Invite invite, bool success)> callback);

    void set_slowmode(const std::string& channel_id,
                      int seconds,
                      std::function<void(bool)> callback = nullptr);

    void clear_slowmode(const std::string& channel_id,
                        std::function<void(bool)> callback = nullptr);

    void update_status(const std::string& text,
                       const std::string& presence = "Online",
                       std::function<void(bool)> callback = nullptr);

    void fetch_channel_permissions(const std::string& channel_id,
                                   std::function<void(nlohmann::json permissions, bool success)> callback);

    void set_channel_permissions(const std::string& channel_id,
                                 const std::string& role_id,
                                 int64_t allow_mask,
                                 int64_t deny_mask,
                                 std::function<void(bool success)> callback = nullptr);

    rest_client& rest();
    gateway& ws();

    std::optional<models::Server>  get_server(const std::string& id) const;
    std::vector<models::Server>    get_servers() const;
    std::optional<models::Channel> get_channel(const std::string& id) const;
    std::optional<models::User>    get_user(const std::string& id) const;

    models::User current_user() const;
    const ClientConfig& config() const;
    const std::string& token() const;
    int64_t ping_latency() const;
    std::chrono::steady_clock::time_point launch_time() const;
    uint64_t uptime() const;
    const std::vector<Command>& get_commands() const;

    bool is_owner(const std::string& user_id) const;
    bool is_server_owner(const std::string& server_id, const std::string& user_id) const;
    bool has_permission(const models::Server& server, const models::Member& member, int64_t permission_mask) const;
    void on_rest_error(std::function<void(const std::string& method, const std::string& path, int status_code, const std::string& error_msg)> cb);
    void on_command_cooldown(std::function<void(cluster&, const events::Message&, const Command&, int64_t remaining_seconds)> cb);
    void on_command_run(std::function<bool(cluster&, const events::Message&, const Command&, const std::optional<models::Member>&, const std::vector<std::string>&)> cb);
    void on_command_error(std::function<void(cluster&, const events::Message&, const Command&, const std::string& error_type)> cb);

    // Cog modules and Commands APIs
    void use(std::unique_ptr<bot_module> module);
    void register_command(const Command& cmd);
    void register_command(const std::string& name,
                          std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb);
    void register_command(const std::string& name,
                          const std::vector<std::string>& aliases,
                          std::function<void(cluster&, const events::Message&, const std::vector<std::string>&)> cb);

    void fetch_user(const std::string& user_id, std::function<void(models::User, bool success)> callback);
    void fetch_member(const std::string& server_id, const std::string& user_id, std::function<void(models::Member, bool success)> callback);
    void await_message(const std::string& channel_id,
                       const std::string& user_id,
                       std::chrono::milliseconds timeout,
                       std::function<void(std::optional<events::Message>)> callback);

    uint64_t start_timer(timer_callback cb, uint64_t interval_seconds);
    bool stop_timer(uint64_t id);

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
    std::vector<Command> registered_commands_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point launch_time_;

    struct HelpSession {
        int current_page = 0;
        int max_pages = 1;
        std::string user_id;
    };
    std::unordered_map<std::string, HelpSession> help_sessions_;
    mutable std::mutex help_sessions_mutex_;

    struct MessageWatcher {
        std::string watcher_id;
        std::string channel_id;
        std::string user_id;
        std::function<void(std::optional<events::Message>)> callback;
    };
    std::vector<MessageWatcher> message_watchers_;
    mutable std::mutex message_watchers_mutex_;

    // Background Timers variables
    struct TimerInfo {
        uint64_t id;
        timer_callback callback;
        uint64_t interval_seconds;
        std::chrono::steady_clock::time_point last_run;
    };
    std::vector<TimerInfo> timers_;
    mutable std::mutex timers_mutex_;
    std::atomic<uint64_t> next_timer_id_{1};
    std::thread timer_thread_;
    std::atomic<bool> timers_running_{false};
    std::condition_variable timer_cv_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::chrono::steady_clock::time_point>> command_cooldowns_;
    mutable std::mutex cooldowns_mutex_;
    void run_timer_loop();
    void dispatch_command(events::Message msg);

    std::function<void(cluster&, const events::Message&, const Command&, int64_t remaining_seconds)> command_cooldown_handler_ = nullptr;
    std::function<void(const std::string& method, const std::string& path, int status_code, const std::string& error_msg)> rest_error_handler_ = nullptr;
    std::function<bool(cluster&, const events::Message&, const Command&, const std::optional<models::Member>&, const std::vector<std::string>&)> command_run_handler_ = nullptr;
    std::function<void(cluster&, const events::Message&, const Command&, const std::string& error_type)> command_error_handler_ = nullptr;
};

} // namespace stoatpp
