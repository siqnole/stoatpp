#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#include <functional>

namespace stoatpp {

class pulse_voice_client {
public:
    using audio_callback_t = std::function<void(const int16_t* samples, size_t count)>;

    pulse_voice_client();
    ~pulse_voice_client();

    // Prevent copy/move to avoid double frees of PulseAudio streams
    pulse_voice_client(const pulse_voice_client&) = delete;
    pulse_voice_client& operator=(const pulse_voice_client&) = delete;
    pulse_voice_client(pulse_voice_client&&) noexcept;
    pulse_voice_client& operator=(pulse_voice_client&&) noexcept;

    // Initialize the PulseAudio connection
    bool init(const std::string& app_name, const std::string& stream_name);
    
    // Close PulseAudio streams
    void shutdown();

    // Play a generated sine wave tone locally via PulseAudio
    bool play_tone(double frequency_hz, double duration_s);

    // Play a join/leave chime sound locally via PulseAudio
    bool play_chime(bool is_join);

    // Write raw PCM audio data (mono, 48kHz, S16LE) to the playback stream
    bool write_audio(const void* data, size_t bytes);

    // Set an audio callback to capture outbound samples (e.g. for WebRTC)
    void set_audio_monitor(audio_callback_t cb);

    // Wait for all written audio to finish playing
    bool drain();

    // Record audio from the microphone for a duration, returns raw PCM samples (mono, 48kHz, S16LE)
    // Computes the average volume level (Root Mean Square / RMS) of the recording.
    std::vector<int16_t> record_mic(double duration_s, double& average_rms);

    // Check if the client has been initialized
    bool is_initialized() const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace stoatpp
