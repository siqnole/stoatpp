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

    // --- Command Tweaks (Optional) ---
    config.dispatch_commands_on_edit = true;  // Run commands when existing command messages are edited
    config.case_insensitive_commands = true;  // Match commands and aliases case-insensitively

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

stoatpp::models::Embed embed;
embed.title       = "hello";
embed.colour      = "#5865f2";
embed.set_image("https://example.com/banner.png");
payload.embeds.push_back(embed.to_json());

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

## embeds

`stoatpp::models::Embed` is a typed builder for Revolt's sendable embed.
call `.to_json()` and push the result into `MessagePayload::embeds`.

> **Note:** bot-sent embeds use the `SendableEmbed` API type, which does not
> support a dedicated image field. To display an image, embed it as markdown
> in the `description` field: `![](url)`.

```cpp
stoatpp::models::Embed embed;
embed.title       = "stoat++ embed";
embed.description = "supports **markdown** and `code`";
embed.url         = "https://github.com/stoat-chat";  // link on the title
embed.icon_url    = "https://example.com/icon.png";   // small icon
embed.colour      = "#5865f2";                         // CSS colour

// embed an image via markdown in the description
embed.set_image("https://example.com/banner.png");     // appends \n![](url)

// or manually:
embed.description = "some text\n![](https://example.com/image.png)";

stoatpp::models::MessagePayload payload;
payload.content = "check this out:";
payload.embeds.push_back(embed.to_json());
bot.send_message("channel_id", payload);
```

**`Embed` fields**

| field | type | description |
|---|---|---|
| `title` | `optional<string>` | embed heading |
| `description` | `optional<string>` | body text (markdown supported, use `![](url)` for images) |
| `url` | `optional<string>` | URL the title links to |
| `icon_url` | `optional<string>` | small icon shown next to the title |
| `colour` | `optional<string>` | CSS colour string (e.g. `"#ff6b6b"`) |

**`set_image(url)`** — convenience method that appends `\n![](url)` to `description`.

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

// set channel slowmode (in seconds)
bot.set_slowmode("channel_id", 5, [](bool success) {
    if (success) std::cout << "slowmode set" << std::endl;
});

// clear channel slowmode
bot.clear_slowmode("channel_id", [](bool success) {
    if (success) std::cout << "slowmode cleared" << std::endl;
});

// update bot presence/status
bot.update_status("playing stoat++!", "Online", [](bool success) {
    if (success) std::cout << "status updated" << std::endl;
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
bot.rest().get_webhook("webhook_id");
bot.rest().edit_webhook("webhook_id", {{"name", "updated"}});
bot.rest().delete_webhook("webhook_id");

stoatpp::models::WebhookExecutePayload wh;
wh.content = "hello";
bot.rest().execute_webhook("webhook_id", "token", wh);
bot.rest().execute_webhook_with_token("webhook_id", "token", wh);
bot.rest().get_webhook_with_token("webhook_id", "token");
bot.rest().edit_webhook_with_token("webhook_id", "token", {{"name", "renamed"}});
bot.rest().delete_webhook_with_token("webhook_id", "token");
bot.rest().edit_webhook_message("webhook_id", "token", "message_id", {{"content", "new content"}});
bot.rest().delete_webhook_message("webhook_id", "token", "message_id");
bot.rest().execute_github_webhook("webhook_id", "token", {{"action", "opened"}});

bot.rest().get_attachment_metadata("attachment_id");
```

### messages & reactions
```cpp
stoatpp::models::MessageQuery query;
query.limit = 50;
bot.rest().get_messages("channel_id", query);

stoatpp::models::MessageSearchQuery search_query;
search_query.query = "my search term";
bot.rest().search_messages("channel_id", search_query);

bot.rest().delete_messages_bulk("channel_id", {"id1", "id2"});
bot.rest().pin_message("channel_id", "msg_id");
bot.rest().unpin_message("channel_id", "msg_id");
bot.rest().get_pinned_messages("channel_id");
bot.rest().add_reaction("channel_id", "msg_id", "👍");
bot.rest().remove_reaction("channel_id", "msg_id", "👍");
bot.rest().clear_reactions("channel_id", "msg_id");
bot.rest().acknowledge_message("channel_id", "msg_id");
bot.rest().acknowledge_channel("channel_id");
```

### channels & permissions
```cpp
bot.rest().get_channel("channel_id");
bot.rest().set_channel_default_permission("channel_id", 1024);
```

### servers & moderation
```cpp
bot.rest().get_server("server_id");
bot.rest().create_server("my server");
bot.rest().edit_server("server_id", {{"name", "new name"}});
bot.rest().leave_server("server_id");

bot.rest().get_server_member("server_id", "user_id");
bot.rest().edit_member("server_id", "user_id", {{"nickname", "new nick"}});
bot.rest().get_server_emojis("server_id");

bot.rest().ban_user("server_id", "user_id", "spam");
bot.rest().unban_user("server_id", "user_id");
bot.rest().create_role("server_id", "admin");
```

### users & relationships
```cpp
bot.rest().get_user("user_id");
bot.rest().get_user_profile("user_id");
bot.rest().get_user_default_avatar("user_id");
bot.rest().get_user_flags("user_id");
bot.rest().change_username("new_username");
```

### auth, mfa & onboarding
```cpp
bot.rest().change_password("old_pass", "new_pass");
bot.rest().change_email("pass", "new_email@revolt.chat");
bot.rest().reset_password_apply("reset_token", "new_pass");
bot.rest().delete_all_sessions(false);

bot.rest().get_mfa_methods();
bot.rest().get_mfa_recovery_codes();
bot.rest().regenerate_mfa_recovery_codes();

bot.rest().get_onboarding_status();
bot.rest().complete_onboarding("my_username");
```

### groups, voice & bots
```cpp
bot.rest().create_group("my group", {"user_1", "user_2"});
bot.rest().get_group_members("channel_id");
bot.rest().join_voice_call("channel_id");
bot.rest().end_voice_ring("channel_id", "user_id");

bot.rest().get_owned_bots();
bot.rest().get_bot_invite_info("bot_id");
```

### invites
```cpp
bot.rest().get_invite("invite_code");
bot.rest().accept_invite("invite_code");
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

## Examples

For complete, runnable example bots demonstrating the library's capabilities, check the `examples/` directory:

1. **[Ping Bot](file:///home/siq/stoat++/examples/ping_bot/main.cpp)**: A simple gateway-based bot that responds with "pong!" to `!ping` messages.
2. **[Full Bot](file:///home/siq/stoat++/examples/full_bot/main.cpp)**: Demonstrates registering commands using `register_command`, handling logging callbacks, tracking member join events, and basic typing indicator status updates.
3. **[Moderation Bot](file:///home/siq/stoat++/examples/moderation_bot/main.cpp)**: Shows how to perform moderation actions (banning, kicking, and adding roles) with commands, parse user mentions, and resolve server role caches.
4. **[Reaction Roles Bot](file:///home/siq/stoat++/examples/reaction_roles_bot/main.cpp)**: Listens to gateway `on_message_react` and `on_message_unreact` events to dynamically assign and remove roles based on member reactions.
5. **[Custom Prefix Bot](file:///home/siq/stoat++/examples/custom_prefix_bot/main.cpp)**: Demonstrates utilizing the custom `prefix_resolver` configuration callback to dynamically resolve custom command prefixes on a per-server basis.

