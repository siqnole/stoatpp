#include <stoatpp/stoatpp.h>
#include <iostream>
#include <unordered_map>

// In-memory mock database of per-server prefixes
std::unordered_map<std::string, std::string> server_prefixes = {
    {"01FSERVER123456789ABCDEFGHJ", "?"},
    {"01FSERVER987654321ZYXWVUTSR", "$"}
};

int main() {
    stoatpp::ClientConfig cfg;
    cfg.log_level = stoatpp::LogLevel::INFO;
    cfg.command_prefix = "!"; // default fallback prefix

    // Set custom prefix resolver callback
    cfg.prefix_resolver = [](stoatpp::cluster& bot, const stoatpp::events::Message& msg) -> std::vector<std::string> {
        // If it's a server message, check if the server has a custom prefix
        if (!msg.server_id.empty()) {
            auto it = server_prefixes.find(msg.server_id);
            if (it != server_prefixes.end()) {
                // Support both custom prefix and fallback default prefix
                return {it->second, bot.config().command_prefix};
            }
        }
        return {bot.config().command_prefix};
    };

    stoatpp::cluster bot("YOUR_BOT_TOKEN_HERE", cfg);

    bot.on_ready([](const stoatpp::events::Ready& ev) {
        std::cout << "Custom Prefix Bot Ready! Logged in as: " << ev.user.username << "\n";
    });

    // Register a command that can be triggered using the resolved prefix
    bot.register_command("hello", [](stoatpp::cluster& cl, const stoatpp::events::Message& msg, const std::vector<std::string>& args) {
        cl.send_message(msg.channel_id, "Hello! Your command was parsed using the prefix: `" + msg.prefix + "`");
    });

    bot.start();
    return 0;
}
