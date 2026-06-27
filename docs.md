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
