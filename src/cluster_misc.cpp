#include "stoatpp/cluster.h"
#include "stoatpp/utils/logger.h"
#include <thread>
#include <chrono>

namespace stoatpp {

rest_client& cluster::rest() { return rest_; }
gateway&     cluster::ws()   { return gateway_; }

const ClientConfig& cluster::config() const { return config_; }
const std::string&  cluster::token()  const { return token_; }
int64_t             cluster::ping_latency() const { return gateway_.ping_latency(); }

std::chrono::steady_clock::time_point cluster::launch_time() const { return launch_time_; }

uint64_t cluster::uptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - launch_time_).count();
}

const std::vector<Command>& cluster::get_commands() const { return registered_commands_; }

bool cluster::is_owner(const std::string& user_id) const {
    return !config_.owner_id.empty() && config_.owner_id == user_id;
}

bool cluster::is_server_owner(const std::string& server_id, const std::string& user_id) const {
    auto server = get_server(server_id);
    return server && server->owner == user_id;
}

bool cluster::has_permission(const models::Server& server,
                             const models::Member& member,
                             int64_t permission_mask) const {
    if (server.owner == member.id) return true;
    int64_t allowed = 0;
    if (server.raw.contains("default_permissions")) {
        const auto& dp = server.raw["default_permissions"];
        if (dp.is_number()) {
            allowed |= dp.get<int64_t>();
        } else if (dp.is_object() && dp.contains("server") && dp["server"].is_number()) {
            allowed |= dp["server"].get<int64_t>();
        }
    }
    for (const auto& role_id : member.roles) {
        for (const auto& r : server.roles) {
            if (r.id == role_id) allowed |= r.allowed_permissions();
        }
    }
    return (allowed & permission_mask) == permission_mask;
}

void cluster::on_rest_error(
    std::function<void(const std::string&, const std::string&, int, const std::string&)> cb) {
    rest_error_handler_ = cb;
}

void cluster::update_status(const std::string& text,
                           const std::string& presence,
                           std::function<void(bool)> callback) {
    std::thread([this, text, presence, callback]() {
        try {
            nlohmann::json status_obj;
            status_obj["text"] = text;
            status_obj["presence"] = presence;
            nlohmann::json body;
            body["status"] = status_obj;
            auto res = rest_.edit_current_user(body);
            if (callback) callback(res.success());
        } catch (const std::exception& e) {
            utils::logger::log(LogLevel::ERROR,
                "Exception in update_status: " + std::string(e.what()), config_);
            if (callback) callback(false);
        }
    }).detach();
}

void cluster::fetch_user(const std::string& user_id,
                         std::function<void(models::User, bool)> callback) {
    auto cached = get_user(user_id);
    if (cached && cached->bio.has_value()) {
        if (callback) callback(*cached, true);
        return;
    }
    std::thread([this, user_id, cached, callback]() {
        try {
            models::User usr;
            bool got_user = false;
            if (cached) { usr = *cached; got_user = true; }
            if (!got_user) {
                auto res = rest_.get_user(user_id);
                if (res.success()) { usr = models::User::from_json(res.body); got_user = true; }
            }
            if (!got_user) { if (callback) callback(models::User{}, false); return; }

            auto prof_res = rest_.get_user_profile(user_id);
            if (prof_res.success()) {
                const auto& prof = prof_res.body;
                if (prof.contains("content") && prof["content"].is_string())
                    usr.bio = prof["content"].get<std::string>();
                if (prof.contains("background") && !prof["background"].is_null()) {
                    if (prof["background"].is_string()) {
                        usr.banner = prof["background"].get<std::string>();
                    } else if (prof["background"].is_object()) {
                        if (prof["background"].contains("id") && prof["background"]["id"].is_string())
                            usr.banner = prof["background"]["id"].get<std::string>();
                        else if (prof["background"].contains("_id") && prof["background"]["_id"].is_string())
                            usr.banner = prof["background"]["_id"].get<std::string>();
                    }
                }
            } else {
                usr.bio = "";
                usr.banner = "";
            }
            {
                std::lock_guard<std::shared_mutex> lock(cache_mutex_);
                user_cache_[usr.id] = usr;
            }
            if (callback) callback(usr, true);
        } catch (...) {
            if (callback) callback(models::User{}, false);
        }
    }).detach();
}

void cluster::fetch_member(const std::string& server_id,
                           const std::string& user_id,
                           std::function<void(models::Member, bool)> callback) {
    std::thread([this, server_id, user_id, callback]() {
        try {
            auto res = rest_.get_server_member(server_id, user_id);
            if (res.success()) {
                if (callback) callback(models::Member::from_json(res.body), true);
            } else {
                if (callback) callback(models::Member{}, false);
            }
        } catch (...) {
            if (callback) callback(models::Member{}, false);
        }
    }).detach();
}

uint64_t cluster::start_timer(timer_callback cb, uint64_t interval_seconds) {
    uint64_t id = next_timer_id_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(timers_mutex_);
        timers_.push_back({id, cb, interval_seconds, std::chrono::steady_clock::now()});
    }
    timer_cv_.notify_all();
    return id;
}

bool cluster::stop_timer(uint64_t id) {
    std::lock_guard<std::mutex> lock(timers_mutex_);
    auto it = std::find_if(timers_.begin(), timers_.end(),
        [id](const TimerInfo& t) { return t.id == id; });
    if (it != timers_.end()) { timers_.erase(it); return true; }
    return false;
}

void cluster::run_timer_loop() {
    while (timers_running_) {
        std::vector<std::pair<timer_callback, timer>> to_run;
        {
            std::lock_guard<std::mutex> lock(timers_mutex_);
            auto now = std::chrono::steady_clock::now();
            for (auto& t : timers_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - t.last_run).count();
                if (elapsed >= static_cast<int64_t>(t.interval_seconds)) {
                    to_run.push_back({t.callback, {t.id, t.interval_seconds}});
                    t.last_run = now;
                }
            }
        }
        for (const auto& run : to_run) {
            if (run.first) {
                try {
                    run.first(run.second);
                } catch (const std::exception& e) {
                    utils::logger::log(LogLevel::ERROR,
                        "Exception in timer callback: " + std::string(e.what()), config_);
                } catch (...) {
                    utils::logger::log(LogLevel::ERROR,
                        "Unknown exception in timer callback", config_);
                }
            }
        }
        std::unique_lock<std::mutex> cv_lock(timers_mutex_);
        timer_cv_.wait_for(cv_lock, std::chrono::milliseconds(500),
            [this]() { return !timers_running_; });
    }
}

} // namespace stoatpp
