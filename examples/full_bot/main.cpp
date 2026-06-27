#include <stoatpp/stoatpp.h>
#include <iostream>
#include <map>
#include <thread>
#include <chrono>

// Command handler map: prefix "!"
std::map<std::string,
    std::function<void(stoatpp::cluster&, const stoatpp::events::Message&)>
> commands;

void register_commands() {
    commands["ping"] = [](stoatpp::cluster& bot, const stoatpp::events::Message& msg) {
        bot.send_message(msg.channel_id, "Pong!");
    };

    commands["info"] = [](stoatpp::cluster& bot, const stoatpp::events::Message& msg) {
        stoatpp::models::MessagePayload p;
        p.content = "stoat++ bot — built with the stoat++ C++ library by siqnole";
        bot.send_message(msg.channel_id, p);
    };

    commands["typing"] = [](stoatpp::cluster& bot, const stoatpp::events::Message& msg) {
        bot.begin_typing(msg.channel_id);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        bot.end_typing(msg.channel_id);
        bot.send_message(msg.channel_id, "Done typing!");
    };
}

int main() {
    stoatpp::ClientConfig cfg;
    cfg.log_level = stoatpp::LogLevel::DEBUG;
    cfg.cache_messages = true;
    cfg.ws_ping_interval_ms = 20000;
    cfg.ready_fields = {"users", "servers", "channels"};

    // Pipe logs to stderr with custom format
    cfg.log_callback = [](stoatpp::LogLevel lvl, const std::string& msg) {
        const char* label = lvl == stoatpp::LogLevel::ERROR   ? "ERR"
                          : lvl == stoatpp::LogLevel::WARNING ? "WRN"
                          : lvl == stoatpp::LogLevel::DEBUG   ? "DBG"
                          :                                     "INF";
        std::fprintf(stderr, "[stoat++ %s] %s\n", label, msg.c_str());
    };

    stoatpp::cluster bot("your_bot_token_here", cfg);

    register_commands();

    bot.on_ready([](const stoatpp::events::Ready& e) {
        std::cout << "Ready! Serving " << e.servers.size() << " servers.\n";
    });

    bot.on_message([&bot](const stoatpp::events::Message& msg) {
        if (msg.author.bot || msg.content.empty()) return;
        if (msg.content[0] != '!') return;

        std::string cmd = msg.content.substr(1);
        auto it = commands.find(cmd);
        if (it != commands.end()) {
            it->second(bot, msg);
        }
    });

    bot.on_server_member_join([&bot](const stoatpp::events::ServerMemberJoin& e) {
        auto srv = bot.get_server(e.server_id);
        if (!srv) return;
        std::cout << "User " << e.user_id << " joined " << srv->name << "\n";
    });

    bot.on_raw_event([](const std::string& type, const nlohmann::json& data) {
        // Uncomment to log every event:
        // std::cout << "[RAW] " << type << "\n";
    });

    bot.on_logout([](const stoatpp::events::Logout&) {
        std::cerr << "Bot was logged out! Check if token was reset.\n";
    });

    bot.start();
    return 0;
}
