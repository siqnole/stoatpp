#include "stoatpp/voice_connection.h"
#include <livekit/livekit.h>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace stoatpp {

namespace {
std::once_flag g_livekit_init_flag;
} // namespace

struct voice_connection::impl {
    std::unique_ptr<livekit::Room> room;
    std::shared_ptr<livekit::AudioSource> audio_source;
    std::shared_ptr<livekit::LocalAudioTrack> audio_track;

    std::atomic<bool> connected{false};
    std::atomic<bool> running{false};

    std::thread send_thread;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::queue<int16_t> pcm_queue;

    void start_send_thread() {
        running = true;
        send_thread = std::thread([this]() {
            constexpr int kSampleRate = 48000;
            constexpr int kChannels = 1;
            constexpr size_t kFrameSamples = 480; // 10 ms at 48 kHz mono

            while (running) {
                std::vector<int16_t> chunk;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    queue_cv.wait(lock, [this]() {
                        return !running || pcm_queue.size() >= kFrameSamples;
                    });
                    if (!running) break;

                    chunk.reserve(kFrameSamples);
                    for (size_t i = 0; i < kFrameSamples; ++i) {
                        chunk.push_back(pcm_queue.front());
                        pcm_queue.pop();
                    }
                }

                if (audio_source && connected.load()) {
                    try {
                        livekit::AudioFrame frame(std::move(chunk), kSampleRate, kChannels, static_cast<int>(kFrameSamples));
                        audio_source->captureFrame(frame, 0);
                    } catch (const std::exception& e) {
                        std::cerr << "[stoat++ voice] captureFrame error: " << e.what() << "\n";
                    }
                }
            }
        });
    }

    void stop_send_thread() {
        running = false;
        queue_cv.notify_all();
        if (send_thread.joinable()) {
            send_thread.join();
        }
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!pcm_queue.empty()) pcm_queue.pop();
    }
};

voice_connection::voice_connection() : pimpl_(std::make_unique<impl>()) {
    std::call_once(g_livekit_init_flag, []() {
        livekit::initialize(livekit::LogLevel::Warn);
    });
}

voice_connection::~voice_connection() {
    disconnect();
}

bool voice_connection::connect(const std::string& ws_url, const std::string& token) {
    disconnect();

    std::string url = ws_url;
    if (url.rfind("https://", 0) == 0) {
        url.replace(0, 8, "wss://");
    } else if (url.rfind("http://", 0) == 0) {
        url.replace(0, 7, "ws://");
    }

    try {
        pimpl_->room = std::make_unique<livekit::Room>();

        livekit::RoomOptions options;
        options.auto_subscribe = false;

        std::cout << "[stoat++ voice] Connecting to LiveKit room: " << url << "\n";
        if (!pimpl_->room->connect(url, token, options)) {
            std::cerr << "[stoat++ voice] LiveKit Room::connect failed\n";
            pimpl_->room.reset();
            return false;
        }

        // 48 kHz mono, 200 ms internal buffer for smooth non-realtime/TTS/music streaming
        pimpl_->audio_source = std::make_shared<livekit::AudioSource>(48000, 1, 200);

        auto lp = pimpl_->room->localParticipant().lock();
        if (!lp) {
            std::cerr << "[stoat++ voice] Failed to lock localParticipant\n";
            pimpl_->room->disconnect();
            pimpl_->room.reset();
            pimpl_->audio_source.reset();
            return false;
        }

        pimpl_->audio_track = lp->publishAudioTrack(
            "stoatpp-audio",
            pimpl_->audio_source,
            livekit::TrackSource::SOURCE_MICROPHONE
        );

        pimpl_->connected = true;
        pimpl_->start_send_thread();
        std::cout << "[stoat++ voice] LiveKit connected and audio track published!\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[stoat++ voice] LiveKit connect exception: " << e.what() << "\n";
        disconnect();
        return false;
    }
}

void voice_connection::disconnect() {
    if (!pimpl_) return;
    pimpl_->connected = false;
    pimpl_->stop_send_thread();

    if (pimpl_->room) {
        try {
            pimpl_->room->disconnect();
        } catch (...) {}
    }

    pimpl_->audio_track.reset();
    pimpl_->audio_source.reset();
    pimpl_->room.reset();
}

bool voice_connection::is_connected() const {
    return pimpl_ && pimpl_->connected.load();
}

bool voice_connection::write_pcm(const int16_t* samples, size_t count) {
    if (!is_connected() || !samples || count == 0) return false;
    {
        std::lock_guard<std::mutex> lock(pimpl_->queue_mutex);
        // Cap queue at ~2 seconds (96000 samples) to avoid unbounded lag
        if (pimpl_->pcm_queue.size() > 96000) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            pimpl_->pcm_queue.push(samples[i]);
        }
    }
    pimpl_->queue_cv.notify_one();
    return true;
}

} // namespace stoatpp
