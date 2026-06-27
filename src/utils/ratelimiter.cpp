#include "stoatpp/utils/ratelimiter.h"

namespace stoatpp::utils {

void ratelimiter::update(const std::string& bucket,
                         int limit,
                         int remaining,
                         int reset_after_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    BucketState& state = buckets_[bucket];
    state.limit = limit;
    state.remaining = remaining;
    state.reset_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(reset_after_ms);
}

int ratelimiter::check(const std::string& bucket) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(bucket);
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

    return std::chrono::duration_cast<std::chrono::milliseconds>(state.reset_at - now).count();
}

bool ratelimiter::is_limited(const std::string& bucket) const {
    return check(bucket) > 0;
}

} // namespace stoatpp::utils
