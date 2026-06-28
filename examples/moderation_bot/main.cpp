#include <stoatpp/stoatpp.h>
#include <iostream>
#include <sstream>
#include <algorithm>

// Simple helper to parse user ID from mention like <@01FH...>
std::string parse_mention(const std::string& arg) {
    if (arg.rfind("<@", 0) == 0 && arg.back() == '>') {
        return arg.substr(2, arg.length() - 3);
    }
    return arg;
}

int main() {
    stoatpp::ClientConfig cfg;
    cfg.log_level = stoatpp::LogLevel::INFO;

    stoatpp::cluster bot("YOUR_BOT_TOKEN_HERE", cfg);

    bot.on_ready([](const stoatpp::events::Ready& ev) {
        std::cout << "Moderation Bot Ready! Logged in as: " << ev.user.username << "\n";
    });

    // !kick <user>
    bot.register_command("kick", [](stoatpp::cluster& cl, const stoatpp::events::Message& msg, const std::vector<std::string>& args) {
        if (args.empty()) {
            cl.send_message(msg.channel_id, "Usage: `!kick <user>`");
            return;
        }

        std::string target_id = parse_mention(args[0]);
        cl.kick_member(msg.server_id, target_id, [&cl, msg, target_id](bool success) {
            if (success) {
                cl.send_message(msg.channel_id, "Successfully kicked <@" + target_id + ">.");
            } else {
                cl.send_message(msg.channel_id, "Failed to kick member. Check hierarchy and permissions.");
            }
        });
    });

    // !ban <user> [reason...]
    bot.register_command("ban", [](stoatpp::cluster& cl, const stoatpp::events::Message& msg, const std::vector<std::string>& args) {
        if (args.empty()) {
            cl.send_message(msg.channel_id, "Usage: `!ban <user> [reason]`");
            return;
        }

        std::string target_id = parse_mention(args[0]);
        std::string reason = "Banned by mod";
        if (args.size() > 1) {
            reason = "";
            for (size_t i = 1; i < args.size(); ++i) {
                reason += args[i] + (i + 1 < args.size() ? " " : "");
            }
        }

        cl.ban_user(msg.server_id, target_id, reason, [&cl, msg, target_id](bool success) {
            if (success) {
                cl.send_message(msg.channel_id, "Successfully banned <@" + target_id + ">.");
            } else {
                cl.send_message(msg.channel_id, "Failed to ban user.");
            }
        });
    });

    // !roleadd <user> <role_id_or_name>
    bot.register_command("roleadd", [](stoatpp::cluster& cl, const stoatpp::events::Message& msg, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cl.send_message(msg.channel_id, "Usage: `!roleadd <user> <role_id_or_name>`");
            return;
        }

        std::string target_id = parse_mention(args[0]);
        std::string role_query = args[1];

        // Resolve role ID from name or ID
        auto srv = cl.get_server(msg.server_id);
        if (!srv) {
            cl.send_message(msg.channel_id, "Server not found in cache.");
            return;
        }

        std::string role_id;
        for (const auto& role : srv->roles) {
            if (role.id == role_query || role.name == role_query) {
                role_id = role.id;
                break;
            }
        }

        if (role_id.empty()) {
            cl.send_message(msg.channel_id, "Role not found: `" + role_query + "`");
            return;
        }

        cl.add_role_to_member(msg.server_id, target_id, role_id, [&cl, msg, target_id, role_query](bool success) {
            if (success) {
                cl.send_message(msg.channel_id, "Added role `" + role_query + "` to <@" + target_id + ">.");
            } else {
                cl.send_message(msg.channel_id, "Failed to add role.");
            }
        });
    });

    bot.start();
    return 0;
}
