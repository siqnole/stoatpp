#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace stoatpp::utils {

class ratelimiter {
public:
    static std::string normalize_route(const std::string& method, const std::string& path);

    void update(const std::string& bucket,
                int limit,
                int remaining,
                int reset_after_ms,
                const std::string& route_key = "");

    // Returns 0 and reserves 1 call if allowed; otherwise returns ms to wait.
    int acquire(const std::string& route_key);

    // Records a 429 backoff shared across all threads using this route/bucket.
    void mark_limited(const std::string& route_key,
                      const std::string& bucket,
                      int retry_after_ms);

    int check(const std::string& bucket) const;

    int remaining(const std::string& bucket) const;

    bool is_limited(const std::string& bucket) const;

private:
    struct BucketState {
        int limit = 1;
        int remaining = 1;
        std::chrono::steady_clock::time_point reset_at;
    };

    std::string resolve_bucket_locked(const std::string& key) const;

    std::unordered_map<std::string, BucketState> buckets_;
    std::unordered_map<std::string, std::string> route_to_bucket_;
    mutable std::mutex mutex_;
};

} // namespace stoatpp::utils
