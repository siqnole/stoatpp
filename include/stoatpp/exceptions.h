#pragma once
#include <stdexcept>
#include <string>

namespace stoatpp {

class StoatException : public std::runtime_error {
public:
    explicit StoatException(const std::string& msg) : std::runtime_error(msg) {}
};

class AuthException : public StoatException {
public:
    using StoatException::StoatException;
};

class RateLimitError : public StoatException {
public:
    int retry_after_ms;
    RateLimitError(const std::string& msg, int retry_ms)
        : StoatException(msg), retry_after_ms(retry_ms) {}
};

class NetworkError : public StoatException {
public:
    using StoatException::StoatException;
};

class APIError : public StoatException {
public:
    int http_status;
    APIError(const std::string& msg, int status)
        : StoatException(msg), http_status(status) {}
};

} // namespace stoatpp
