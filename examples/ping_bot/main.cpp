#include <stoatpp/stoatpp.h>
#include <iostream>

int main() {
    stoatpp::ClientConfig cfg;
    cfg.log_level = stoatpp::LogLevel::INFO;

    stoatpp::cluster bot("your_bot_token_here", cfg);

    bot.on_ready([](const stoatpp::events::Ready& event) {
        std::cout << "[Ready] Logged in as " << event.user.username << "\n";
    });

    bot.on_message([&bot](const stoatpp::events::Message& event) {
        // Ignore bots
        if (event.author.bot) return;

        if (event.content == "!ping") {
            bot.send_message(event.channel_id, "Pong! 🏓");
        }

        if (event.content == "!hello") {
            stoatpp::models::MessagePayload payload;
            payload.content = "Hello, " + event.author.username + "!";
            payload.replies.push_back({event.id, true});
            bot.send_message(event.channel_id, payload);
        }
    });

    bot.on_error([](const stoatpp::events::Error& e) {
        std::cerr << "[Gateway Error] " << e.error_id << "\n";
    });

    bot.start();  // blocks
    return 0;
}
