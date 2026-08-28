#include "stoatpp/pulse_voice.h"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

namespace stoatpp {

struct pulse_voice_client::impl {
    pa_simple* s_play = nullptr;
    pa_simple* s_rec = nullptr;
    pa_sample_spec ss;
    bool initialized = false;
    std::string app_name;
    std::string stream_name;
    audio_callback_t monitor_cb = nullptr;
};

pulse_voice_client::pulse_voice_client() : pimpl_(std::make_unique<impl>()) {}

pulse_voice_client::~pulse_voice_client() {
    shutdown();
}

pulse_voice_client::pulse_voice_client(pulse_voice_client&&) noexcept = default;
pulse_voice_client& pulse_voice_client::operator=(pulse_voice_client&&) noexcept = default;

bool pulse_voice_client::init(const std::string& app_name, const std::string& stream_name) {
    pimpl_->app_name = app_name;
    pimpl_->stream_name = stream_name;
    
    // Set format parameters: 16-bit signed, mono, 48000 Hz
    pimpl_->ss.format = PA_SAMPLE_S16LE;
    pimpl_->ss.channels = 1;
    pimpl_->ss.rate = 48000;
    
    pimpl_->initialized = true;
    return true;
}

void pulse_voice_client::shutdown() {
    if (pimpl_->s_play) {
        pa_simple_free(pimpl_->s_play);
        pimpl_->s_play = nullptr;
    }
    if (pimpl_->s_rec) {
        pa_simple_free(pimpl_->s_rec);
        pimpl_->s_rec = nullptr;
    }
    pimpl_->initialized = false;
}

bool pulse_voice_client::is_initialized() const {
    return pimpl_->initialized;
}

bool pulse_voice_client::play_tone(double frequency_hz, double duration_s) {
    if (!pimpl_->initialized) return false;

    size_t num_samples = static_cast<size_t>(pimpl_->ss.rate * duration_s);
    std::vector<int16_t> samples(num_samples);
    
    // Generate sine wave
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / pimpl_->ss.rate;
        samples[i] = static_cast<int16_t>(16384.0 * std::sin(2.0 * M_PI * frequency_hz * t));
    }
    
    if (!write_audio(samples.data(), samples.size() * sizeof(int16_t))) {
        return false;
    }
    return drain();
}

bool pulse_voice_client::play_chime(bool is_join) {
    if (!pimpl_->initialized) return false;
    
    if (is_join) {
        // High-pitched double beep for joining (Arpeggio style)
        play_tone(880.0, 0.15);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        play_tone(1109.73, 0.25); // C#6
    } else {
        // Lower double beep for leaving
        play_tone(587.33, 0.15); // D5
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        play_tone(440.0, 0.25); // A4
    }
    return true;
}

bool pulse_voice_client::write_audio(const void* data, size_t bytes) {
    if (!pimpl_->initialized) return false;
    
    if (pimpl_->monitor_cb) {
        pimpl_->monitor_cb(reinterpret_cast<const int16_t*>(data), bytes / sizeof(int16_t));
    }

    int error = 0;
    if (!pimpl_->s_play) {
        pimpl_->s_play = pa_simple_new(
            nullptr,
            pimpl_->app_name.c_str(),
            PA_STREAM_PLAYBACK,
            nullptr,
            pimpl_->stream_name.c_str(),
            &pimpl_->ss,
            nullptr,
            nullptr,
            &error
        );
        if (!pimpl_->s_play) {
            std::cerr << "[stoatpp pulse] Failed to create PulseAudio playback stream: " 
                      << pa_strerror(error) << std::endl;
            return false;
        }
    }
    
    if (pa_simple_write(pimpl_->s_play, data, bytes, &error) < 0) {
        std::cerr << "[stoatpp pulse] Failed to write to PulseAudio playback: " 
                  << pa_strerror(error) << std::endl;
        return false;
    }
    
    return true;
}

void pulse_voice_client::set_audio_monitor(audio_callback_t cb) {
    pimpl_->monitor_cb = cb;
}

bool pulse_voice_client::drain() {
    if (!pimpl_->initialized || !pimpl_->s_play) return false;
    
    int error = 0;
    if (pa_simple_drain(pimpl_->s_play, &error) < 0) {
        std::cerr << "[stoatpp pulse] Failed to drain PulseAudio stream: " 
                  << pa_strerror(error) << std::endl;
        return false;
    }
    
    return true;
}

std::vector<int16_t> pulse_voice_client::record_mic(double duration_s, double& average_rms) {
    std::vector<int16_t> samples;
    average_rms = 0.0;
    
    if (!pimpl_->initialized) return samples;
    
    int error = 0;
    if (!pimpl_->s_rec) {
        pimpl_->s_rec = pa_simple_new(
            nullptr,
            pimpl_->app_name.c_str(),
            PA_STREAM_RECORD,
            nullptr,
            pimpl_->stream_name.c_str(),
            &pimpl_->ss,
            nullptr,
            nullptr,
            &error
        );
        if (!pimpl_->s_rec) {
            std::cerr << "[stoatpp pulse] Failed to create PulseAudio record stream: " 
                      << pa_strerror(error) << std::endl;
            return samples;
        }
    }
    
    size_t total_samples = static_cast<size_t>(pimpl_->ss.rate * duration_s);
    samples.resize(total_samples);
    
    // Read recording samples
    if (pa_simple_read(pimpl_->s_rec, samples.data(), samples.size() * sizeof(int16_t), &error) < 0) {
        std::cerr << "[stoatpp pulse] Failed to read from PulseAudio recording: " 
                  << pa_strerror(error) << std::endl;
        return samples;
    }
    
    // Calculate RMS
    double sum_sq = 0.0;
    for (int16_t sample : samples) {
        double norm = static_cast<double>(sample) / 32768.0;
        sum_sq += norm * norm;
    }
    average_rms = std::sqrt(sum_sq / samples.size());
    
    return samples;
}

} // namespace stoatpp
