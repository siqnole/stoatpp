#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace stoatpp {

class IHttpClient;

enum class LogLevel {
    NONE = 0,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    TRACE
};

enum class TokenType {
    BOT,   // sends X-Bot-Token header
    USER   // sends X-Session-Token header
};

struct ClientConfig {
    // --- Authentication ---
    TokenType token_type = TokenType::BOT;

    // --- Endpoints (override for self-hosted instances) ---
    std::string api_base_url   = "https://api.stoat.chat";
    std::string ws_url         = "wss://events.stoat.chat";
    int         ws_version     = 1;                  // ?version=1
    std::string ws_format      = "json";             // "json" or "msgpack"

    // --- WebSocket behaviour ---
    int  ws_ping_interval_ms   = 15000;              // ping every 15s (10–30s recommended)
    int  ws_reconnect_delay_ms = 5000;               // delay before reconnect
    bool ws_auto_reconnect     = true;
    bool ws_auth_in_url        = false;              // Auth via Authenticate event by default

    // --- Ready payload fields (empty = all fields) ---
    std::vector<std::string> ready_fields = {};

    // --- REST ---
    int  http_timeout_ms       = 10000;
    int  http_retry_count      = 3;
    bool http_respect_ratelimits = true;
    int  ratelimit_retry_delay_ms = 500;             // wait before retry on 429

    // --- Logging ---
    LogLevel log_level         = LogLevel::INFO;
    // Custom log callback: override the built-in logger entirely
    // Signature: void(LogLevel, const std::string& message)
    std::function<void(LogLevel, const std::string&)> log_callback = nullptr;

    // --- Threading ---
    int event_thread_count     = 1;                  // threads for dispatching events
    int rest_thread_count      = 2;                  // threads for HTTP requests

    // --- Cache ---
    bool cache_messages        = false;              // off by default
    int  message_cache_size    = 1000;               // max messages per channel cached
    bool cache_members         = true;
    bool cache_channels        = true;
    bool cache_servers         = true;

    // --- Custom HTTP headers ---
    std::map<std::string, std::string> extra_headers = {};

    // --- Command prefix (like discord.py) ---
    std::string command_prefix = "!";

    // --- Intents / Subscriptions (future-proofing) ---
    bool subscribe_to_user_updates = false;

    // --- Extension Hooks ---
    std::function<std::unique_ptr<IHttpClient>()> http_client_factory = nullptr;
};

} // namespace stoatpp
