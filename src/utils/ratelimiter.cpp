#include "stoatpp/utils/ratelimiter.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace stoatpp::utils {

std::string ratelimiter::normalize_route(const std::string& method, const std::string& path) {
    auto qpos = path.find('?');
    std::string clean = (qpos == std::string::npos) ? path : path.substr(0, qpos);

    std::vector<std::string> parts;
    std::stringstream ss(clean);
    std::string item;
    while (std::getline(ss, item, '/')) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }

    // Keep major parameters (channels/{id}, servers/{id}, bots/{id}) so different
    // channels/servers get independent buckets, while collapsing minor IDs
    // (messages/{id}, members/{id}, roles/{id}, reactions/{emoji}, etc.).
    std::string normalized;
    for (size_t i = 0; i < parts.size(); ++i) {
        normalized += '/';
        if (i > 0) {
            const std::string& prev = parts[i - 1];
            bool is_major = (prev == "channels" || prev == "servers" || prev == "bots");
            bool is_minor_parent = (prev == "messages" || prev == "members" ||
                                    prev == "roles" || prev == "invites" ||
                                    prev == "bans" || prev == "emojis" ||
                                    prev == "webhooks" || prev == "users");
            if (!is_major && is_minor_parent && parts[i] != "@me") {
                normalized += "{id}";
                continue;
            }
            // Collapse reaction emoji segments after /reactions/
            if (prev == "reactions") {
                normalized += "{emoji}";
                continue;
            }
        }
        normalized += parts[i];
    }
    if (normalized.empty()) normalized = "/";
    return method + ":" + normalized;
}

std::string ratelimiter::resolve_bucket_locked(const std::string& key) const {
    auto r_it = route_to_bucket_.find(key);
    if (r_it != route_to_bucket_.end()) {
        return r_it->second;
    }
    return key;
}

void ratelimiter::update(const std::string& bucket,
                         int limit,
                         int remaining,
                         int reset_after_ms,
                         const std::string& route_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!route_key.empty() && !bucket.empty()) {
        route_to_bucket_[route_key] = bucket;
    }
    const std::string& key = bucket.empty() ? route_key : bucket;
    if (key.empty()) return;

    BucketState& state = buckets_[key];
    state.limit = std::max(1, limit);
    state.remaining = std::max(0, remaining);
    state.reset_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(0, reset_after_ms));
}

int ratelimiter::acquire(const std::string& route_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string bucket = resolve_bucket_locked(route_key);
    auto it = buckets_.find(bucket);
    if (it == buckets_.end()) {
        return 0;
    }

    BucketState& state = it->second;
    auto now = std::chrono::steady_clock::now();
    if (now >= state.reset_at) {
        state.remaining = std::max(1, state.limit);
    }

    if (state.remaining > 0) {
        --state.remaining;
        return 0;
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(state.reset_at - now).count();
    return static_cast<int>(std::max<int64_t>(1, ms));
}

void ratelimiter::mark_limited(const std::string& route_key,
                               const std::string& bucket,
                               int retry_after_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!route_key.empty() && !bucket.empty()) {
        route_to_bucket_[route_key] = bucket;
    }
    const std::string& key = bucket.empty() ? route_key : bucket;
    if (key.empty()) return;

    BucketState& state = buckets_[key];
    state.remaining = 0;
    auto target_reset = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(1, retry_after_ms));
    if (target_reset > state.reset_at) {
        state.reset_at = target_reset;
    }
}

int ratelimiter::check(const std::string& bucket) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string resolved = resolve_bucket_locked(bucket);
    auto it = buckets_.find(resolved);
    if (it == buckets_.end()) {
        return 0;
    }

    const BucketState& state = it->second;
    if (state.remaining > 0) {
        return 0;
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= state.reset_at) {
        return 0;
    }

    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(state.reset_at - now).count());
}

int ratelimiter::remaining(const std::string& bucket) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string resolved = resolve_bucket_locked(bucket);
    auto it = buckets_.find(resolved);
    if (it == buckets_.end()) {
        return 10;
    }
    if (std::chrono::steady_clock::now() >= it->second.reset_at) {
        return std::max(1, it->second.limit);
    }
    return it->second.remaining;
}

bool ratelimiter::is_limited(const std::string& bucket) const {
    return check(bucket) > 0;
}

} // namespace stoatpp::utils
