#include "stoatpp/gateway.h"
#include "stoatpp/rest.h"
#include "stoatpp/utils/logger.h"
#include <ixwebsocket/IXWebSocket.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace stoatpp {

struct gateway::impl {
    ix::WebSocket ws;
    std::thread ping_thread;
    std::atomic<bool> ping_thread_running{false};
    std::condition_variable ping_cv;
    std::mutex ping_mutex;
    std::atomic<bool> authenticated{false};
    std::atomic<int64_t> latency_ms{0};
    models::User self_user;
};

gateway::gateway(const std::string& token, const ClientConfig& config,
                 event_dispatcher& dispatcher)
    : token_(token), config_(config), dispatcher_(dispatcher), pimpl_(std::make_unique<impl>()) {
    
    pimpl_->ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            on_message_received(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            on_connected();
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            on_disconnected();
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::string err_msg = "WebSocket Error: " + msg->errorInfo.reason;
            utils::logger::log(LogLevel::ERROR, err_msg, config_);
        }
    });

    pimpl_->ws.enableAutomaticReconnection();
}

gateway::~gateway() {
    disconnect();
}

void gateway::connect() {
    try {
        rest_client rest(token_, config_);
        auto res = rest.get("/users/@me");
        if (res.success()) {
            pimpl_->self_user = models::User::from_json(res.body);
            utils::logger::log(LogLevel::INFO, "Gateway fetched self user: " + pimpl_->self_user.username + " (" + pimpl_->self_user.id + ")", config_);
        } else {
            utils::logger::log(LogLevel::ERROR, "Gateway failed to fetch self user: " + res.error_message(), config_);
        }
    } catch (const std::exception& e) {
        utils::logger::log(LogLevel::ERROR, "Gateway exception fetching self user: " + std::string(e.what()), config_);
    }

    std::string url = config_.ws_url;
    if (url.empty() || url.back() != '/') {
        url += "/";
    }
    url += "?version=" + std::to_string(config_.ws_version) + "&format=" + config_.ws_format;
    if (config_.ws_auth_in_url) {
        url += "&token=" + token_;
    }
    
    utils::logger::log(LogLevel::INFO, "Connecting to Gateway: " + url, config_);
    pimpl_->ws.setUrl(url);
    pimpl_->ws.start();
}

void gateway::disconnect() {
    if (pimpl_->ping_thread_running) {
        pimpl_->ping_thread_running = false;
        pimpl_->ping_cv.notify_all();
        if (pimpl_->ping_thread.joinable()) {
            pimpl_->ping_thread.join();
        }
    }

    if (is_connected()) {
        pimpl_->ws.stop();
    }
    pimpl_->authenticated = false;
}

bool gateway::is_connected() const {
    return pimpl_->ws.getReadyState() == ix::ReadyState::Open;
}

void gateway::send_raw(const nlohmann::json& payload) {
    if (!is_connected()) {
        utils::logger::log(LogLevel::WARNING, "Attempted to send payload when WebSocket is not connected: " + payload.dump(), config_);
        return;
    }
    utils::logger::log(LogLevel::TRACE, "WS Send: " + payload.dump(), config_);
    pimpl_->ws.send(payload.dump());
}

void gateway::authenticate(const std::string& token) {
    utils::logger::log(LogLevel::INFO, "Sending Authenticate event...", config_);
    send_raw(nlohmann::json{
        {"type", "Authenticate"},
        {"token", token}
    });
}

void gateway::ping(int64_t data) {
    send_raw(nlohmann::json{
        {"type", "Ping"},
        {"data", data}
    });
}

void gateway::begin_typing(const std::string& channel_id) {
    send_raw(nlohmann::json{
        {"type", "BeginTyping"},
        {"channel", channel_id}
    });
}

void gateway::end_typing(const std::string& channel_id) {
    send_raw(nlohmann::json{
        {"type", "EndTyping"},
        {"channel", channel_id}
    });
}

void gateway::subscribe(const std::string& server_id) {
    send_raw(nlohmann::json{
        {"type", "Subscribe"},
        {"server_id", server_id}
    });
}

void gateway::on_message_received(const std::string& raw) {
    utils::logger::log(LogLevel::TRACE, "WS Recv: " + raw, config_);
    try {
        nlohmann::json j = nlohmann::json::parse(raw);
        handle_event(j);
    } catch (const std::exception& e) {
        utils::logger::log(LogLevel::ERROR, "Failed to parse WS payload: " + std::string(e.what()), config_);
    }
}

void gateway::on_connected() {
    utils::logger::log(LogLevel::INFO, "WebSocket connection established.", config_);
    if (!config_.ws_auth_in_url) {
        authenticate(token_);
    } else {
        pimpl_->authenticated = true;
        schedule_ping();
    }
}

void gateway::on_disconnected() {
    utils::logger::log(LogLevel::INFO, "WebSocket connection closed.", config_);
    pimpl_->authenticated = false;
    
    if (pimpl_->ping_thread_running) {
        pimpl_->ping_thread_running = false;
        pimpl_->ping_cv.notify_all();
        if (pimpl_->ping_thread.joinable()) {
            pimpl_->ping_thread.join();
        }
    }
    
    dispatcher_.dispatch_logout({});
}

void gateway::schedule_ping() {
    if (pimpl_->ping_thread_running) return;
    
    pimpl_->ping_thread_running = true;
    pimpl_->ping_thread = std::thread([this]() {
        utils::logger::log(LogLevel::DEBUG, "Starting Gateway heartbeat thread.", config_);
        while (pimpl_->ping_thread_running) {
            std::unique_lock<std::mutex> lock(pimpl_->ping_mutex);
            if (pimpl_->ping_cv.wait_for(lock, std::chrono::milliseconds(config_.ws_ping_interval_ms), [this]() {
                return !pimpl_->ping_thread_running;
            })) {
                break;
            }
            
            if (is_connected() && pimpl_->authenticated) {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                ping(now_ms);
            }
        }
        utils::logger::log(LogLevel::DEBUG, "Gateway heartbeat thread exiting.", config_);
    });
}

void gateway::handle_event(const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("type") || !j["type"].is_string()) return;
    
    std::string type = j["type"].get<std::string>();
    
    dispatcher_.dispatch_raw_event(type, j);

    if (type == "Bulk") {
        if (j.contains("v") && j["v"].is_array()) {
            for (const auto& ev : j["v"]) {
                handle_event(ev);
            }
        }
        return;
    }
    
    if (type == "Authenticated") {
        utils::logger::log(LogLevel::INFO, "Handshake completed. Authenticated successfully.", config_);
        pimpl_->authenticated = true;
        schedule_ping();
        return;
    }
    
    if (type == "Pong") {
        if (j.contains("data") && j["data"].is_number()) {
            auto pong_time = j["data"].get<int64_t>();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            pimpl_->latency_ms = now_ms - pong_time;
            utils::logger::log(LogLevel::DEBUG, "Gateway heartbeat latency updated: " + std::to_string(pimpl_->latency_ms.load()) + "ms", config_);
        }
        return;
    }
    
    if (type == "Error") {
        events::Error ev;
        if (j.contains("error") && j["error"].is_string()) ev.error_id = j["error"].get<std::string>();
        else if (j.contains("error_id") && j["error_id"].is_string()) ev.error_id = j["error_id"].get<std::string>();
        dispatcher_.dispatch_error(ev);
        return;
    }
    
    if (type == "Ready") {
        events::Ready ev;
        if (j.contains("user")) {
            ev.user = models::User::from_json(j["user"]);
        } else {
            bool found_self = false;
            if (j.contains("users") && j["users"].is_array() && !pimpl_->self_user.id.empty()) {
                for (const auto& u : j["users"]) {
                    std::string uid;
                    if (u.contains("id") && u["id"].is_string()) uid = u["id"].get<std::string>();
                    else if (u.contains("_id") && u["_id"].is_string()) uid = u["_id"].get<std::string>();
                    
                    if (uid == pimpl_->self_user.id) {
                        ev.user = models::User::from_json(u);
                        found_self = true;
                        break;
                    }
                }
            }
            if (!found_self) {
                ev.user = pimpl_->self_user;
            }
        }
        
        if (j.contains("servers") && j["servers"].is_array()) {
            for (const auto& s : j["servers"]) {
                ev.servers.push_back(models::Server::from_json(s));
            }
        }
        if (j.contains("channels") && j["channels"].is_array()) {
            for (const auto& c : j["channels"]) {
                ev.channels.push_back(models::Channel::from_json(c));
            }
        }
        if (j.contains("members") && j["members"].is_array()) {
            for (const auto& m : j["members"]) {
                ev.members.push_back(models::Member::from_json(m));
            }
        }
        if (j.contains("emojis") && j["emojis"].is_array()) {
            for (const auto& em : j["emojis"]) {
                ev.emojis.push_back(models::Emoji::from_json(em));
            }
        }
        dispatcher_.dispatch_ready(ev);
        return;
    }
    
    if (type == "Message") {
        events::Message ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        else if (j.contains("_id")) ev.id = j["_id"].get<std::string>();
        
        if (j.contains("channel")) ev.channel_id = j["channel"].get<std::string>();
        if (j.contains("server")) {
            ev.server_id = j["server"].get<std::string>();
        } else if (j.contains("member") && j["member"].is_object() && j["member"].contains("_id") && j["member"]["_id"].is_object() && j["member"]["_id"].contains("server")) {
            ev.server_id = j["member"]["_id"]["server"].get<std::string>();
        }
        
        if (j.contains("user") && j["user"].is_object()) {
            ev.author = models::User::from_json(j["user"]);
        } else if (j.contains("author")) {
            if (j["author"].is_object()) {
                ev.author = models::User::from_json(j["author"]);
            } else if (j["author"].is_string()) {
                ev.author.id = j["author"].get<std::string>();
            }
        }
        
        if (j.contains("content")) ev.content = j["content"].get<std::string>();
        if (j.contains("nonce")) ev.nonce = j["nonce"].get<std::string>();
        if (j.contains("edited") && j["edited"].is_boolean()) ev.edited = j["edited"].get<bool>();
        
        ev.raw = j;
        dispatcher_.dispatch_message(ev);
        return;
    }

    if (type == "MessageUpdate") {
        events::MessageUpdate ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("channel")) ev.channel_id = j["channel"].get<std::string>();
        if (j.contains("data")) ev.data = j["data"];
        dispatcher_.dispatch_message_update(ev);
        return;
    }

    if (type == "MessageDelete") {
        events::MessageDelete ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("channel")) ev.channel_id = j["channel"].get<std::string>();
        dispatcher_.dispatch_message_delete(ev);
        return;
    }

    if (type == "MessageReact") {
        events::MessageReact ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("channel")) ev.channel_id = j["channel"].get<std::string>();
        if (j.contains("user_id")) ev.user_id = j["user_id"].get<std::string>();
        if (j.contains("emoji_id")) ev.emoji_id = j["emoji_id"].get<std::string>();
        dispatcher_.dispatch_message_react(ev);
        return;
    }

    if (type == "MessageUnreact") {
        events::MessageUnreact ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("channel")) ev.channel_id = j["channel"].get<std::string>();
        if (j.contains("user_id")) ev.user_id = j["user_id"].get<std::string>();
        if (j.contains("emoji_id")) ev.emoji_id = j["emoji_id"].get<std::string>();
        dispatcher_.dispatch_message_unreact(ev);
        return;
    }

    if (type == "ChannelCreate") {
        events::ChannelCreate ev;
        ev.channel = models::Channel::from_json(j);
        ev.raw = j;
        dispatcher_.dispatch_channel_create(ev);
        return;
    }

    if (type == "ChannelUpdate") {
        events::ChannelUpdate ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("data")) ev.data = j["data"];
        if (j.contains("clear") && j["clear"].is_array()) {
            for (const auto& c : j["clear"]) {
                if (c.is_string()) ev.clear.push_back(c.get<std::string>());
            }
        }
        dispatcher_.dispatch_channel_update(ev);
        return;
    }

    if (type == "ChannelDelete") {
        events::ChannelDelete ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        dispatcher_.dispatch_channel_delete(ev);
        return;
    }

    if (type == "ServerCreate") {
        events::ServerCreate ev;
        ev.server = models::Server::from_json(j);
        ev.raw = j;
        dispatcher_.dispatch_server_create(ev);
        return;
    }

    if (type == "ServerUpdate") {
        events::ServerUpdate ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("data")) ev.data = j["data"];
        if (j.contains("clear") && j["clear"].is_array()) {
            for (const auto& c : j["clear"]) {
                if (c.is_string()) ev.clear.push_back(c.get<std::string>());
            }
        }
        dispatcher_.dispatch_server_update(ev);
        return;
    }

    if (type == "ServerDelete") {
        events::ServerDelete ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        dispatcher_.dispatch_server_delete(ev);
        return;
    }

    if (type == "ServerMemberJoin") {
        events::ServerMemberJoin ev;
        if (j.contains("id")) ev.server_id = j["id"].get<std::string>();
        if (j.contains("user")) ev.user_id = j["user"].get<std::string>();
        if (j.contains("member")) {
            ev.member = models::Member::from_json(j["member"]);
        } else {
            ev.member = models::Member::from_json(j);
        }
        dispatcher_.dispatch_server_member_join(ev);
        return;
    }

    if (type == "ServerMemberLeave") {
        events::ServerMemberLeave ev;
        if (j.contains("id")) ev.server_id = j["id"].get<std::string>();
        if (j.contains("user")) ev.user_id = j["user"].get<std::string>();
        dispatcher_.dispatch_server_member_leave(ev);
        return;
    }

    if (type == "ServerMemberUpdate") {
        events::ServerMemberUpdate ev;
        if (j.contains("id")) {
            if (j["id"].is_object()) {
                if (j["id"].contains("server")) ev.server_id = j["id"]["server"].get<std::string>();
                if (j["id"].contains("user")) ev.user_id = j["id"]["user"].get<std::string>();
            }
        }
        if (j.contains("data")) ev.data = j["data"];
        if (j.contains("clear") && j["clear"].is_array()) {
            for (const auto& c : j["clear"]) {
                if (c.is_string()) ev.clear.push_back(c.get<std::string>());
            }
        }
        dispatcher_.dispatch_server_member_update(ev);
        return;
    }

    if (type == "UserUpdate") {
        events::UserUpdate ev;
        if (j.contains("id")) ev.id = j["id"].get<std::string>();
        if (j.contains("data")) ev.data = j["data"];
        if (j.contains("clear") && j["clear"].is_array()) {
            for (const auto& c : j["clear"]) {
                if (c.is_string()) ev.clear.push_back(c.get<std::string>());
            }
        }
        dispatcher_.dispatch_user_update(ev);
        return;
    }

    if (type == "ChannelStartTyping") {
        events::ChannelStartTyping ev;
        if (j.contains("id")) ev.channel_id = j["id"].get<std::string>();
        if (j.contains("user")) ev.user_id = j["user"].get<std::string>();
        dispatcher_.dispatch_channel_start_typing(ev);
        return;
    }

    if (type == "ChannelStopTyping") {
        events::ChannelStopTyping ev;
        if (j.contains("id")) ev.channel_id = j["id"].get<std::string>();
        if (j.contains("user")) ev.user_id = j["user"].get<std::string>();
        dispatcher_.dispatch_channel_stop_typing(ev);
        return;
    }
}

int64_t gateway::ping_latency() const {
    return pimpl_->latency_ms.load();
}

} // namespace stoatpp
