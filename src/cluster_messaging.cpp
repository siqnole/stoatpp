#include "stoatpp/cluster.h"
#include "stoatpp/utils/logger.h"
#include <thread>
#include <atomic>
#include <algorithm>

namespace stoatpp {

void cluster::send_message(const std::string& channel_id,
                           const std::string& content,
                           std::function<void(models::Message, bool success)> callback) {
    models::MessagePayload payload;
    payload.content = content;
    send_message(channel_id, payload, callback);
}

void cluster::send_reply(const std::string& channel_id,
                         const std::string& message_id,
                         const std::string& content,
                         bool mention,
                         std::function<void(models::Message, bool success)> callback) {
    models::MessagePayload payload;
    payload.content = content;
    payload.replies.push_back({message_id, mention});
    send_message(channel_id, payload, callback);
}

void cluster::send_reply(const events::Message& to_message,
                         const std::string& content,
                         bool mention,
                         std::function<void(models::Message, bool success)> callback) {
    send_reply(to_message.channel_id, to_message.id, content, mention, callback);
}

void cluster::send_message(const std::string& channel_id,
                           const models::MessagePayload& payload_raw,
                           std::function<void(models::Message, bool)> callback) {
    models::MessagePayload payload = payload_raw;
    if (config_.masquerade_handler) {
        config_.masquerade_handler(*this, channel_id, payload);
    }
    std::thread([this, channel_id, payload, callback]() {
        try {
            auto res = rest_.create_message(channel_id, payload);
            if (res.success()) {
                auto msg = models::Message::from_json(res.body);
                if (callback) callback(msg, true);
                if (payload.delete_after > 0) {
                    std::string msg_id = msg.id;
                    int delay = payload.delete_after;
                    std::thread([this, channel_id, msg_id, delay]() {
                        std::this_thread::sleep_for(std::chrono::seconds(delay));
                        rest_.delete_message(channel_id, msg_id);
                    }).detach();
                }
            } else {
                utils::logger::log(LogLevel::ERROR,
                    "Failed to send message: " + res.error_message() + " | body: " + res.body.dump(), config_);
                if (callback) callback(models::Message{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in send_message: " + std::string(e.what()), config_);
            if (callback) callback(models::Message{}, false);
        }
    }).detach();
}

void cluster::edit_message(const std::string& channel_id,
                           const std::string& message_id,
                           const std::string& new_content,
                           std::function<void(models::Message, bool)> callback) {
    std::thread([this, channel_id, message_id, new_content, callback]() {
        try {
            nlohmann::json body = {{"content", new_content}};
            auto res = rest_.edit_message(channel_id, message_id, body);
            if (res.success()) {
                if (callback) callback(models::Message::from_json(res.body), true);
            } else {
                utils::logger::log(LogLevel::ERROR,
                    "Failed to edit message: " + res.error_message(), config_);
                if (callback) callback(models::Message{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in edit_message: " + std::string(e.what()), config_);
            if (callback) callback(models::Message{}, false);
        }
    }).detach();
}

void cluster::edit_message(const std::string& channel_id,
                           const std::string& message_id,
                           const models::MessagePayload& payload,
                           std::function<void(models::Message, bool)> callback) {
    std::thread([this, channel_id, message_id, payload, callback]() {
        try {
            auto res = rest_.edit_message(channel_id, message_id, payload.to_json());
            if (res.success()) {
                if (callback) callback(models::Message::from_json(res.body), true);
            } else {
                utils::logger::log(LogLevel::ERROR,
                    "Failed to edit message: " + res.error_message(), config_);
                if (callback) callback(models::Message{}, false);
            }
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in edit_message: " + std::string(e.what()), config_);
            if (callback) callback(models::Message{}, false);
        }
    }).detach();
}

void cluster::delete_message(const std::string& channel_id,
                             const std::string& message_id,
                             std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, callback]() {
        try {
            auto res = rest_.delete_message(channel_id, message_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in delete_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::pin_message(const std::string& channel_id,
                          const std::string& message_id,
                          std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, callback]() {
        try {
            auto res = rest_.pin_message(channel_id, message_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in pin_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::unpin_message(const std::string& channel_id,
                            const std::string& message_id,
                            std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, callback]() {
        try {
            auto res = rest_.unpin_message(channel_id, message_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in unpin_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::react_to_message(const std::string& channel_id,
                               const std::string& message_id,
                               const std::string& emoji_id,
                               std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, emoji_id, callback]() {
        try {
            auto res = rest_.add_reaction(channel_id, message_id, emoji_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in react_to_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::unreact_from_message(const std::string& channel_id,
                                   const std::string& message_id,
                                   const std::string& emoji_id,
                                   const std::optional<std::string>& user_id,
                                   std::function<void(bool)> callback) {
    std::thread([this, channel_id, message_id, emoji_id, user_id, callback]() {
        try {
            auto res = rest_.remove_reaction(channel_id, message_id, emoji_id, user_id);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in unreact_from_message: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::begin_typing(const std::string& channel_id) {
    gateway_.begin_typing(channel_id);
}

void cluster::end_typing(const std::string& channel_id) {
    gateway_.end_typing(channel_id);
}

void cluster::await_message(const std::string& channel_id,
                            const std::string& user_id,
                            std::chrono::milliseconds timeout,
                            std::function<void(std::optional<events::Message>)> callback) {
    static std::atomic<uint64_t> watcher_counter{0};
    std::string watcher_id = "watcher_" + std::to_string(watcher_counter.fetch_add(1));
    {
        std::lock_guard<std::mutex> lock(message_watchers_mutex_);
        message_watchers_.push_back({watcher_id, channel_id, user_id, callback});
    }
    std::thread([this, watcher_id, timeout, callback]() {
        std::this_thread::sleep_for(timeout);
        bool triggered = false;
        {
            std::lock_guard<std::mutex> lock(message_watchers_mutex_);
            auto it = std::find_if(message_watchers_.begin(), message_watchers_.end(),
                [&watcher_id](const MessageWatcher& w) { return w.watcher_id == watcher_id; });
            if (it != message_watchers_.end()) {
                message_watchers_.erase(it);
                triggered = true;
            }
        }
        if (triggered && callback) callback(std::nullopt);
    }).detach();
}

} // namespace stoatpp
