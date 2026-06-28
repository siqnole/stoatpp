#include <stoatpp/stoatpp.h>
#include <iostream>
#include <string>

// Configuration for reaction role mapping
const std::string TARGET_MESSAGE_ID = "01FHMSG123456789ABCDEFGHJK";
const std::string TARGET_EMOJI = "👍";
const std::string ASSIGNED_ROLE_ID = "01FHROLE123456789ABCDEFGHJ";

int main() {
    stoatpp::ClientConfig cfg;
    cfg.log_level = stoatpp::LogLevel::INFO;

    stoatpp::cluster bot("YOUR_BOT_TOKEN_HERE", cfg);

    bot.on_ready([](const stoatpp::events::Ready& ev) {
        std::cout << "Reaction Roles Bot Ready! Logged in as: " << ev.user.username << "\n";
    });

    // Listen to reactions added
    bot.on_message_react([&bot](const stoatpp::events::MessageReact& ev) {
        // Verify it matches target message and target emoji
        if (ev.id == TARGET_MESSAGE_ID && ev.emoji_id == TARGET_EMOJI) {
            std::cout << "[reaction_roles] Adding role to user: " << ev.user_id << "\n";
            // Resolve server ID using channel cache
            auto ch = bot.get_channel(ev.channel_id);
            if (ch && ch->server) {
                std::string server_id = *(ch->server);
                bot.add_role_to_member(server_id, ev.user_id, ASSIGNED_ROLE_ID, [](bool success) {
                    if (success) std::cout << "  Role added successfully.\n";
                    else std::cout << "  Failed to add role.\n";
                });
            }
        }
    });

    // Listen to reactions removed
    bot.on_message_unreact([&bot](const stoatpp::events::MessageUnreact& ev) {
        // Verify it matches target message and target emoji
        if (ev.id == TARGET_MESSAGE_ID && ev.emoji_id == TARGET_EMOJI) {
            std::cout << "[reaction_roles] Removing role from user: " << ev.user_id << "\n";
            // Resolve server ID using channel cache
            auto ch = bot.get_channel(ev.channel_id);
            if (ch && ch->server) {
                std::string server_id = *(ch->server);
                bot.remove_role_from_member(server_id, ev.user_id, ASSIGNED_ROLE_ID, [](bool success) {
                    if (success) std::cout << "  Role removed successfully.\n";
                    else std::cout << "  Failed to remove role.\n";
                });
            }
        }
    });

    bot.start();
    return 0;
}
