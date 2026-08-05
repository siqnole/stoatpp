#pragma once
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace stoatpp {

class voice_connection {
public:
    voice_connection();
    ~voice_connection();

    // Prevent copy/move to avoid double-free of WebRTC objects
    voice_connection(const voice_connection&) = delete;
    voice_connection& operator=(const voice_connection&) = delete;
    voice_connection(voice_connection&&) noexcept = delete;
    voice_connection& operator=(voice_connection&&) noexcept = delete;

    // Connect to Revolt voice channel using the LiveKit token and URL
    bool connect(const std::string& ws_url, const std::string& token);

    // Disconnect from the channel
    void disconnect();

    // Check if we are connected
    bool is_connected() const;

    // Send raw PCM audio frames (16-bit, 48kHz, mono).
    // The class will encode these internally to Opus frames and stream them to the WebRTC channel.
    bool write_pcm(const int16_t* samples, size_t count);

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace stoatpp
