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

inline bool is_valid_ulid(const std::string& str) {
    if (str.size() != 26) return false;
    if (str == "00000000000000000000000000") return false;
    const std::string alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZabcdefghjkmnpqrstvwxyz";
    for (char c : str) {
        if (alphabet.find(c) == std::string::npos) {
            return false;
        }
    }
    return true;
}

inline std::string extract_ulid(const std::string& input) {
    if (input.empty()) return "";
    
    std::string candidate = input;
    
    // Strip leading brackets, mentions, spaces
    size_t start = candidate.find_first_not_of("<#@!%& ");
    if (start != std::string::npos) {
        candidate = candidate.substr(start);
    }
    size_t end = candidate.find_last_not_of("> ");
    if (end != std::string::npos) {
        candidate = candidate.substr(0, end + 1);
    }

    // Handle URLs (e.g. https://stt.gg/channel/01M... or https://app.stoat.chat/channel/01M...)
    if (candidate.find('/') != std::string::npos) {
        size_t last_slash = candidate.rfind('/');
        candidate = candidate.substr(last_slash + 1);
    }

    if (is_valid_ulid(candidate)) {
        return candidate;
    }
    return "";
}

} // namespace stoatpp::utils
