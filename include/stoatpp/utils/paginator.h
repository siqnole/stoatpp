#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include "../cluster.h"
#include "../models/message.h"

namespace stoatpp::utils {

class paginator {
private:
    cluster& bot_;
    std::vector<models::Embed> pages_;
    std::string channel_id_;
    std::string message_id_;
    std::string owner_user_id_;
    int current_page_ = 0;
    std::shared_ptr<bool> alive_;

public:
    explicit paginator(cluster& bot)
        : bot_(bot),
          alive_(std::make_shared<bool>(true)) {}

    ~paginator() {
        *alive_ = false;
    }

    paginator& add_page(const models::Embed& page) {
        pages_.push_back(page);
        return *this;
    }

    void send(const std::string& channel_id, const std::string& owner_user_id = "") {
        if (pages_.empty()) return;
        
        channel_id_ = channel_id;
        owner_user_id_ = owner_user_id;
        current_page_ = 0;

        models::MessagePayload payload;
        payload.content = " ";
        payload.embeds.push_back(pages_[0].to_json());

        // Capture a weak pointer or shared state to avoid using 'this' if paginator is destroyed
        auto alive = alive_;

        bot_.send_message(channel_id, payload, [this, alive](models::Message msg, bool success) {
            if (!success || !*alive) return;
            message_id_ = msg.id;

            if (pages_.size() > 1) {
                // Add page navigators
                bot_.react_to_message(channel_id_, message_id_, "⬅");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                bot_.react_to_message(channel_id_, message_id_, "➡");

                // Register listener for reaction events
                bot_.on_message_react([this, alive](const events::MessageReact& e) {
                    if (!*alive) return;
                    if (e.id != message_id_) return;
                    if (e.user_id == bot_.current_user().id) return;
                    
                    // If owner specified, verify it
                    if (!owner_user_id_.empty() && e.user_id != owner_user_id_) {
                        bot_.unreact_from_message(e.channel_id, e.id, e.emoji_id, e.user_id);
                        return;
                    }

                    bool page_changed = false;
                    if (e.emoji_id == "⬅️" || e.emoji_id == "◀" || e.emoji_id == "⬅") {
                        if (current_page_ > 0) {
                            current_page_--;
                        } else {
                            current_page_ = pages_.size() - 1;
                        }
                        page_changed = true;
                    } else if (e.emoji_id == "➡️" || e.emoji_id == "▶" || e.emoji_id == "➡") {
                        if (current_page_ < static_cast<int>(pages_.size()) - 1) {
                            current_page_++;
                        } else {
                            current_page_ = 0;
                        }
                        page_changed = true;
                    }

                    if (page_changed) {
                        models::MessagePayload edit_payload;
                        edit_payload.content = " ";
                        edit_payload.embeds.push_back(pages_[current_page_].to_json());
                        
                        bot_.edit_message(e.channel_id, e.id, edit_payload, [this, e, alive](models::Message, bool) {
                            if (!*alive) return;
                            bot_.unreact_from_message(e.channel_id, e.id, e.emoji_id, e.user_id);
                        });
                    } else {
                        bot_.unreact_from_message(e.channel_id, e.id, e.emoji_id, e.user_id);
                    }
                });
            }
        });
    }
};

} // namespace stoatpp::utils
