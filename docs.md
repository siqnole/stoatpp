# stoat++

a simple c++17 library for stoat.chat.

## installation

add to `CMakeLists.txt`:
```cmake
add_subdirectory(path/to/stoatpp)
target_link_libraries(your_target PRIVATE stoatpp)
```

include:
```cpp
#include <stoatpp/stoatpp.h>
```

## setup & config

```cpp
#include <stoatpp/stoatpp.h>
#include <iostream>

int main() {
    stoatpp::ClientConfig config;
    config.command_prefix = "!";
    config.api_base_url = "https://api.stoat.chat";
    config.token_type = stoatpp::TokenType::BOT; // use TokenType::SESSION for user tokens
    config.http_respect_ratelimits = true; 

    stoatpp::cluster bot("YOUR_TOKEN", config);

    bot.on_ready([](const stoatpp::events::Ready& ev) {
        std::cout << "logged in as " << ev.user.username << std::endl;
    });

    bot.start(false); 
    return 0;
}
```

## commands

register simple prefix commands:

```cpp
bot.register_command("ping", [](stoatpp::cluster& cl, const stoatpp::events::Message& msg, const std::vector<std::string>& args) {
    cl.send_message(msg.channel_id, "pong");
});

bot.register_command("say", [](stoatpp::cluster& cl, const stoatpp::events::Message& msg, const std::vector<std::string>& args) {
    if (args.empty()) return;
    std::string text = "";
    for (const auto& arg : args) {
        text += arg + " ";
    }
    cl.send_message(msg.channel_id, text);
});
```

## message actions

```cpp
// send simple message
bot.send_message("channel_id", "hello");

// send rich message with embeds
stoatpp::models::MessagePayload payload;
payload.content = "embed test";
payload.embeds.push_back({{"title", "hello"}});
bot.send_message("channel_id", payload);

// edit content
bot.edit_message("channel_id", "message_id", "new content");

// edit with payload (embeds/content)
bot.edit_message("channel_id", "message_id", payload);

// delete message
bot.delete_message("channel_id", "message_id");

// add reaction
bot.react_to_message("channel_id", "message_id", "⬅");

// remove reaction (user_id optional)
bot.unreact_from_message("channel_id", "message_id", "⬅", "user_id");
```

## cache

thread-safe local cache query (returns `std::optional<T>`):

```cpp
auto server = bot.get_server("id");
auto channel = bot.get_channel("id");
auto user = bot.get_user("id");
auto me = bot.current_user();
```

async rest fetch for non-cached or full profiles:

```cpp
bot.fetch_user("id", [](stoatpp::models::User user, bool success) {
    if (success) {
        std::cout << user.bio.value_or("") << std::endl;
    }
});

bot.fetch_member("server_id", "user_id", [](stoatpp::models::Member member, bool success) {
    if (success) {
        std::cout << member.nickname.value_or("") << std::endl;
    }
});
```

## moderation

moderation helpers run asynchronously on the `bot` instance:

```cpp
// ban a user
bot.ban_user("server_id", "user_id", "reason", [](bool success) {
    if (success) std::cout << "banned" << std::endl;
});

// unban a user
bot.unban_user("server_id", "user_id", [](bool success) {
    if (success) std::cout << "unbanned" << std::endl;
});

// kick a user
bot.kick_member("server_id", "user_id", [](bool success) {
    if (success) std::cout << "kicked" << std::endl;
});

// timeout a user (takes an ISO timestamp when the timeout ends)
bot.timeout_member("server_id", "user_id", "2034-03-24T09:47:39.470Z", [](bool success) {
    if (success) std::cout << "timed out" << std::endl;
});

// remove timeout
bot.remove_timeout("server_id", "user_id", [](bool success) {
    if (success) std::cout << "timeout removed" << std::endl;
});

// create server role
bot.create_role("server_id", "moderator", [](stoatpp::models::Role role, bool success) {
    if (success) std::cout << "created role: " << role.name << std::endl;
});

// delete server role
bot.delete_role("server_id", "role_id", [](bool success) {
    if (success) std::cout << "role deleted" << std::endl;
});

// add a role to a member
bot.add_role_to_member("server_id", "user_id", "role_id", [](bool success) {
    if (success) std::cout << "role added" << std::endl;
});

// remove a role from a member
bot.remove_role_from_member("server_id", "user_id", "role_id", [](bool success) {
    if (success) std::cout << "role removed" << std::endl;
});

// get member count of a server
bot.get_member_count("server_id", [](int count, bool success) {
    if (success) std::cout << "members: " << count << std::endl;
});

// fetch all server invites
bot.fetch_server_invites("server_id", [](std::vector<stoatpp::models::Invite> invites, bool success) {
    if (success) std::cout << invites.size() << " invites" << std::endl;
});

// create a channel invite
bot.create_invite("channel_id", [](stoatpp::models::Invite invite, bool success) {
    if (success) std::cout << "invite code: " << invite.code << std::endl;
});

// fetch channel permission overrides
bot.fetch_channel_permissions("channel_id", [](nlohmann::json permissions, bool success) {
    if (success) std::cout << permissions.dump() << std::endl;
});

// set channel permissions for a role
bot.set_channel_permissions("channel_id", "role_id", 1024, 0, [](bool success) {
    if (success) std::cout << "permissions updated" << std::endl;
});
```

## rest api

accessible via `bot.rest()`:

### system & utility
```cpp
bot.rest().get_node_info();   // GET /
bot.rest().get_stats();      // GET /stats
bot.rest().get_gateway();    // GET /gateway

stoatpp::models::ReportPayload rpt;
rpt.content_type = "Message";
rpt.id = "msg_id";
rpt.reason = "spam";
bot.rest().create_report(rpt);
```

### webhooks & uploads
```cpp
bot.rest().create_webhook("channel_id", "my web hook");
bot.rest().get_channel_webhooks("channel_id");
bot.rest().delete_webhook("webhook_id");

stoatpp::models::WebhookExecutePayload wh;
wh.content = "hello";
bot.rest().execute_webhook("webhook_id", "token", wh);

bot.rest().get_attachment_metadata("attachment_id");
```

### messages & reactions
```cpp
stoatpp::models::MessageQuery query;
query.limit = 50;
bot.rest().get_messages("channel_id", query);

bot.rest().delete_messages_bulk("channel_id", {"id1", "id2"});
bot.rest().pin_message("channel_id", "msg_id");
bot.rest().unpin_message("channel_id", "msg_id");
bot.rest().get_pinned_messages("channel_id");
```

### servers & moderation
```cpp
bot.rest().create_server("my server");
bot.rest().edit_server("server_id", {{"name", "new name"}});
bot.rest().leave_server("server_id");

bot.rest().ban_user("server_id", "user_id", "spam");
bot.rest().unban_user("server_id", "user_id");
bot.rest().edit_member("server_id", "user_id", {{"nickname", "new nick"}});
bot.rest().create_role("server_id", "admin");
```

## gateway events

register event listeners using `bot.on_<event>`:

```cpp
bot.on_message([](const stoatpp::events::Message& ev) {
    // new message
});

bot.on_message_react([](const stoatpp::events::MessageReact& ev) {
    // reaction added
});

bot.on_user_presence_update([](const stoatpp::events::UserPresenceUpdate& ev) {
    // presence changed
});
```

supported events:
- `on_ready` (`events::Ready`)
- `on_message` (`events::Message`)
- `on_message_update` (`events::MessageUpdate`)
- `on_message_delete` (`events::MessageDelete`)
- `on_message_react` (`events::MessageReact`)
- `on_message_unreact` (`events::MessageUnreact`)
- `on_channel_create` (`events::ChannelCreate`)
- `on_channel_update` (`events::ChannelUpdate`)
- `on_channel_delete` (`events::ChannelDelete`)
- `on_server_create` (`events::ServerCreate`)
- `on_server_update` (`events::ServerUpdate`)
- `on_server_delete` (`events::ServerDelete`)
- `on_server_member_join` (`events::ServerMemberJoin`)
- `on_server_member_leave` (`events::ServerMemberLeave`)
- `on_server_member_update` (`events::ServerMemberUpdate`)
- `on_server_role_create` (`events::ServerRoleCreate`)
- `on_server_role_update` (`events::ServerRoleUpdate`)
- `on_server_role_delete` (`events::ServerRoleDelete`)
- `on_user_update` (`events::UserUpdate`)
- `on_user_presence_update` (`events::UserPresenceUpdate`)
- `on_user_settings_update` (`events::UserSettingsUpdate`)
- `on_channel_start_typing` (`events::ChannelStartTyping`)
- `on_channel_stop_typing` (`events::ChannelStopTyping`)
- `on_webhook_update` (`events::WebhookUpdate`)
- `on_voice_state_update` (`events::VoiceStateUpdate`)
- `on_raw_event` (type, json)
