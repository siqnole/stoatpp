#include "stoatpp/rest.h"

namespace stoatpp {

rest_client::rest_client(const std::string& token, const ClientConfig& config)
    : token_(token), config_(config) {}

std::string rest_client::Response::error_message() const {
    return "HTTP " + std::to_string(status_code);
}

rest_client::Response rest_client::get(const std::string& path) {
    return Response{200, nlohmann::json{}};
}

rest_client::Response rest_client::post(const std::string& path, const nlohmann::json& body) {
    return Response{200, nlohmann::json{}};
}

rest_client::Response rest_client::patch(const std::string& path, const nlohmann::json& body) {
    return Response{200, nlohmann::json{}};
}

rest_client::Response rest_client::put(const std::string& path, const nlohmann::json& body) {
    return Response{200, nlohmann::json{}};
}

rest_client::Response rest_client::del(const std::string& path) {
    return Response{200, nlohmann::json{}};
}

rest_client::Response rest_client::upload_file(const std::string& path,
                                              const std::string& filename,
                                              const std::vector<uint8_t>& data,
                                              const std::string& mime_type) {
    return Response{200, nlohmann::json{}};
}

int rest_client::remaining_calls(const std::string& bucket) const {
    return 9999;
}

int rest_client::reset_after_ms(const std::string& bucket) const {
    return 0;
}

void rest_client::set_pre_request_hook(std::function<void(const std::string& method,
                                                           const std::string& path,
                                                           nlohmann::json& body)> hook) {
    pre_request_hook_ = hook;
}

} // namespace stoatpp
