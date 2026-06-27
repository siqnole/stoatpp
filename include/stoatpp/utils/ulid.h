#pragma once
#include <string>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace stoatpp::utils {

/**
 * Decodes the generation timestamp (millisecond epoch) from a ULID.
 * First 10 Crockford Base32 characters carry the 48-bit timestamp.
 */
inline uint64_t decode_ulid_timestamp(const std::string& ulid) {
    if (ulid.size() < 10) return 0;
    
    const std::string alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    uint64_t timestamp = 0;
    for (int i = 0; i < 10; ++i) {
        char c = std::toupper(static_cast<unsigned char>(ulid[i]));
        size_t pos = alphabet.find(c);
        if (pos == std::string::npos) {
            return 0;
        }
        timestamp = (timestamp * 32) + pos;
    }
    return timestamp;
}

} // namespace stoatpp::utils
