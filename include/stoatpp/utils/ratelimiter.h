#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace stoatpp::utils {

class ratelimiter {
public:
    void update(const std::string& bucket,
                int limit,
                int remaining,
                int reset_after_ms);

    int check(const std::string& bucket) const;

    bool is_limited(const std::string& bucket) const;

private:
    struct BucketState {
        int limit;
        int remaining;
        std::chrono::steady_clock::time_point reset_at;
    };
    std::unordered_map<std::string, BucketState> buckets_;
    mutable std::mutex mutex_;
};

} // namespace stoatpp::utils
