#include "stoatpp/utils/ratelimiter.h"

namespace stoatpp::utils {

void ratelimiter::update(const std::string& bucket,
                         int limit,
                         int remaining,
                         int reset_after_ms) {
    // Stub
}

int ratelimiter::check(const std::string& bucket) const {
    // Stub
    return 0;
}

bool ratelimiter::is_limited(const std::string& bucket) const {
    // Stub
    return false;
}

} // namespace stoatpp::utils
