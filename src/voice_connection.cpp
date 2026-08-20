#include "stoatpp/voice_connection.h"
#include <rtc/rtc.hpp>
#include <rtc/mediahandler.hpp>
#include <opus.h>
#include <ixwebsocket/IXWebSocket.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include <chrono>

namespace stoatpp {

// Minimal Protobuf Writer helper to serialize outbound requests without binary code generator
struct pb_writer {
    std::vector<uint8_t> buffer;

    void write_varint(uint64_t val) {
        while (val >= 0x80) {
            buffer.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
            val >>= 7;
        }
        buffer.push_back(static_cast<uint8_t>(val & 0x7F));
    }

    void write_key(int field_num, int wire_type) {
        write_varint((field_num << 3) | wire_type);
    }

    void write_string(int field_num, const std::string& str) {
        write_key(field_num, 2);
        write_varint(str.size());
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    void write_message(int field_num, const std::vector<uint8_t>& msg_bytes) {
        write_key(field_num, 2);
        write_varint(msg_bytes.size());
        buffer.insert(buffer.end(), msg_bytes.begin(), msg_bytes.end());
    }
};

// Minimal Protobuf Reader helper to parse incoming signals
struct pb_reader {
    const uint8_t* data;
    size_t size;
    size_t offset = 0;

    bool has_more() const { return offset < size; }

    uint64_t read_varint() {
        uint64_t val = 0;
        int shift = 0;
        while (offset < size) {
            uint8_t byte = data[offset++];
            val |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if (!(byte & 0x80)) return val;
            shift += 7;
        }
        return 0;
    }

    std::string read_string() {
        uint64_t len = read_varint();
        if (offset + len > size) return "";
        std::string s(reinterpret_cast<const char*>(data + offset), len);
        offset += len;
        return s;
    }

    void skip_field(int wire_type) {
        if (wire_type == 0) {
            read_varint();
        } else if (wire_type == 2) {
            uint64_t len = read_varint();
            offset += len;
        } else if (wire_type == 1) {
            offset += 8;
        } else if (wire_type == 5) {
            offset += 4;
        }
    }
};

struct IceServerInfo {
    std::vector<std::string> urls;
    std::string username;
    std::string credential;
};

enum SignalTarget {
    PUBLISHER = 0,
    SUBSCRIBER = 1
};

struct voice_connection::impl {
    ix::WebSocket ws;
    std::shared_ptr<rtc::PeerConnection> sub_pc;
    std::shared_ptr<rtc::PeerConnection> pub_pc;
    std::shared_ptr<rtc::Track> audio_track;
    OpusEncoder* encoder = nullptr;

    std::vector<IceServerInfo> ice_servers;

    std::mutex connect_mutex;
    std::condition_variable connect_cv;
    bool joined = false;
    bool failed = false;

    // Send queue and thread variables
    std::thread send_thread;
    std::thread ping_thread;
    std::atomic<bool> ping_running{false};
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::queue<int16_t> pcm_queue;
    bool running = false;

    std::string my_cid = "audio-track-0";
    std::string track_sid;

    impl() {
        rtc::InitLogger(rtc::LogLevel::Debug);
    }

    ~impl() {
        stop_thread();
        if (encoder) {
            opus_encoder_destroy(encoder);
            encoder = nullptr;
        }
    }

    void stop_thread() {
        stop_ping_loop();
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            running = false;
        }
        queue_cv.notify_all();
        if (send_thread.joinable()) {
            send_thread.join();
        }
    }

    void start_ping_loop() {
        stop_ping_loop();
        ping_running = true;
        ping_thread = std::thread([this]() {
            while (ping_running) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (!ping_running) break;
                send_ping();
            }
        });
    }

    void stop_ping_loop() {
        ping_running = false;
        if (ping_thread.joinable()) {
            ping_thread.join();
        }
    }

    void send_ping() {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        pb_writer req;
        req.write_key(10, 0); // field 10: ping (varint timestamp)
        req.write_varint(now);
        ws.sendBinary(std::string(req.buffer.begin(), req.buffer.end()));
    }

    void send_wrapped_join_request() {
        // ClientInfo
        pb_writer info;
        info.write_key(1, 0); // sdk enum
        info.write_varint(5); // CPP
        info.write_string(2, "0.1.0"); // version

        // ConnectionSettings
        pb_writer settings;
        settings.write_key(1, 0); // auto_subscribe
        settings.write_varint(1); // true
        settings.write_key(2, 0); // adaptive_stream
        settings.write_varint(1); // true

        // JoinRequest
        pb_writer join;
        join.write_message(1, info.buffer);
        join.write_message(2, settings.buffer);
        join.write_key(7, 0); // reconnect
        join.write_varint(0); // false

        // WrappedJoinRequest
        pb_writer wrapped;
        wrapped.write_key(1, 0); // compression NONE
        wrapped.write_varint(0);
        wrapped.write_key(2, 2); // join_request bytes
        wrapped.write_varint(join.buffer.size());
        wrapped.buffer.insert(wrapped.buffer.end(), join.buffer.begin(), join.buffer.end());

        ws.sendBinary(std::string(wrapped.buffer.begin(), wrapped.buffer.end()));
    }

    void send_sdp_answer(const std::string& sdp) {
        std::cout << "[stoatpp voice] Sending Subscriber SDP Answer:\n" << sdp << "\n";
        // SessionDescription
        pb_writer sdp_msg;
        sdp_msg.write_string(1, "answer");
        sdp_msg.write_string(2, sdp);

        // SignalRequest (field 2 is answer)
        pb_writer req;
        req.write_message(2, sdp_msg.buffer);

        ws.sendBinary(std::string(req.buffer.begin(), req.buffer.end()));
    }

    void send_sdp_offer(const std::string& sdp) {
        std::cout << "[stoatpp voice] Sending Publisher SDP Offer:\n" << sdp << "\n";
        // SessionDescription
        pb_writer sdp_msg;
        sdp_msg.write_string(1, "offer");
        sdp_msg.write_string(2, sdp);

        // SignalRequest (field 1 is offer)
        pb_writer req;
        req.write_message(1, sdp_msg.buffer);

        ws.sendBinary(std::string(req.buffer.begin(), req.buffer.end()));
    }

    void send_trickle(const std::string& cand_json, SignalTarget target) {
        std::cout << "[stoatpp voice] Sending Trickle Candidate (target=" << target << "): " << cand_json << "\n";
        // TrickleRequest
        pb_writer trickle;
        trickle.write_string(1, cand_json);
        trickle.write_key(2, 0); // target enum
        trickle.write_varint(target);

        // SignalRequest (field 3 is trickle)
        pb_writer req;
        req.write_message(3, trickle.buffer);

        ws.sendBinary(std::string(req.buffer.begin(), req.buffer.end()));
    }

    void send_add_track() {
        std::cout << "[stoatpp voice] Sending AddTrackRequest for CID: " << my_cid << "\n";
        // AddTrackRequest
        pb_writer add_track;
        add_track.write_string(1, my_cid);
        add_track.write_string(2, "audio");
        add_track.write_key(3, 0); // type (AUDIO)
        add_track.write_varint(0);
        add_track.write_key(8, 0); // source (MICROPHONE)
        add_track.write_varint(2);

        // SignalRequest (field 4 is add_track)
        pb_writer req;
        req.write_message(4, add_track.buffer);

        ws.sendBinary(std::string(req.buffer.begin(), req.buffer.end()));
    }

    void handle_binary_message(const std::string& bytes) {
        pb_reader reader{reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()};
        while (reader.has_more()) {
            uint64_t key = reader.read_varint();
            int field_num = key >> 3;
            int wire_type = key & 7;

            if (field_num == 1 && wire_type == 2) { // JoinResponse
                uint64_t len = reader.read_varint();
                pb_reader j_reader{reader.data + reader.offset, len};
                reader.offset += len;

                parse_join_response(j_reader);
            }
            else if (field_num == 3 && wire_type == 2) { // offer (subscriber)
                uint64_t len = reader.read_varint();
                pb_reader sdp_reader{reader.data + reader.offset, len};
                reader.offset += len;

                std::string sdp_type, sdp_text;
                while (sdp_reader.has_more()) {
                    uint64_t sdp_key = sdp_reader.read_varint();
                    int sdp_field = sdp_key >> 3;
                    int sdp_wire = sdp_key & 7;
                    if (sdp_field == 1 && sdp_wire == 2) sdp_type = sdp_reader.read_string();
                    else if (sdp_field == 2 && sdp_wire == 2) sdp_text = sdp_reader.read_string();
                    else sdp_reader.skip_field(sdp_wire);
                }

                std::cout << "[stoatpp voice] Received subscriber SDP offer from server\n";
                if (sub_pc) {
                    sub_pc->setRemoteDescription(rtc::Description(sdp_text, "offer"));
                }
            }
            else if (field_num == 2 && wire_type == 2) { // answer (publisher)
                uint64_t len = reader.read_varint();
                pb_reader sdp_reader{reader.data + reader.offset, len};
                reader.offset += len;

                std::string sdp_type, sdp_text;
                while (sdp_reader.has_more()) {
                    uint64_t sdp_key = sdp_reader.read_varint();
                    int sdp_field = sdp_key >> 3;
                    int sdp_wire = sdp_key & 7;
                    if (sdp_field == 1 && sdp_wire == 2) sdp_type = sdp_reader.read_string();
                    else if (sdp_field == 2 && sdp_wire == 2) sdp_text = sdp_reader.read_string();
                    else sdp_reader.skip_field(sdp_wire);
                }

                std::cout << "[stoatpp voice] Received publisher SDP answer from server\n";
                if (pub_pc) {
                    pub_pc->setRemoteDescription(rtc::Description(sdp_text, "answer"));
                }
            }
            else if (field_num == 4 && wire_type == 2) { // trickle
                uint64_t len = reader.read_varint();
                pb_reader t_reader{reader.data + reader.offset, len};
                reader.offset += len;

                std::string cand_init;
                int target = 0;
                while (t_reader.has_more()) {
                    uint64_t t_key = t_reader.read_varint();
                    int t_field = t_key >> 3;
                    int t_wire = t_key & 7;
                    if (t_field == 1 && t_wire == 2) cand_init = t_reader.read_string();
                    else if (t_field == 2 && t_wire == 0) target = t_reader.read_varint();
                    else t_reader.skip_field(t_wire);
                }

                if (!cand_init.empty()) {
                    try {
                        auto cand_j = nlohmann::json::parse(cand_init);
                        std::string cand_line = cand_j.value("candidate", "");
                        std::string mid = cand_j.value("sdpMid", "");
                        if (!cand_line.empty()) {
                            if (target == SignalTarget::SUBSCRIBER && sub_pc) {
                                sub_pc->addRemoteCandidate(rtc::Candidate(cand_line, mid));
                            } else if (target == SignalTarget::PUBLISHER && pub_pc) {
                                pub_pc->addRemoteCandidate(rtc::Candidate(cand_line, mid));
                            }
                        }
                    } catch (...) {}
                }
            }
            else if (field_num == 6 && wire_type == 2) { // track_published
                uint64_t len = reader.read_varint();
                pb_reader tp_reader{reader.data + reader.offset, len};
                reader.offset += len;

                std::string cid, sid;
                while (tp_reader.has_more()) {
                    uint64_t tp_key = tp_reader.read_varint();
                    int tp_field = tp_key >> 3;
                    int tp_wire = tp_key & 7;
                    if (tp_field == 1 && tp_wire == 2) cid = tp_reader.read_string();
                    else if (tp_field == 2 && tp_wire == 2) {
                        uint64_t t_len = tp_reader.read_varint();
                        pb_reader t_reader{tp_reader.data + tp_reader.offset, t_len};
                        tp_reader.offset += t_len;
                        while (t_reader.has_more()) {
                            uint64_t t_key = t_reader.read_varint();
                            int t_field = t_key >> 3;
                            int t_wire = t_key & 7;
                            if (t_field == 1 && t_wire == 2) sid = t_reader.read_string();
                            else t_reader.skip_field(t_wire);
                        }
                    }
                    else {
                        tp_reader.skip_field(tp_wire);
                    }
                }

                if (cid == my_cid && !sid.empty()) {
                    std::cout << "[stoatpp voice] Track published successfully! Server Assigned SID: " << sid << "\n";
                    track_sid = sid;
                    initialize_publisher_track();
                }
            }
            else {
                reader.skip_field(wire_type);
            }
        }
    }

    void parse_join_response(pb_reader& reader) {
        while (reader.has_more()) {
            uint64_t key = reader.read_varint();
            int field_num = key >> 3;
            int wire_type = key & 7;

            if (field_num == 5 && wire_type == 2) { // ice_servers
                uint64_t len = reader.read_varint();
                pb_reader s_reader{reader.data + reader.offset, len};
                reader.offset += len;

                IceServerInfo s_info;
                while (s_reader.has_more()) {
                    uint64_t s_key = s_reader.read_varint();
                    int s_field = s_key >> 3;
                    int s_wire = s_key & 7;
                    if (s_field == 1 && s_wire == 2) s_info.urls.push_back(s_reader.read_string());
                    else if (s_field == 2 && s_wire == 2) s_info.username = s_reader.read_string();
                    else if (s_field == 3 && s_wire == 2) s_info.credential = s_reader.read_string();
                    else s_reader.skip_field(s_wire);
                }
                ice_servers.push_back(s_info);
            }
            else {
                reader.skip_field(wire_type);
            }
        }

        std::cout << "[stoatpp voice] Join response parsed! Found " << ice_servers.size() << " ICE servers.\n";
        initialize_peer_connections();
        start_ping_loop();

        {
            std::lock_guard<std::mutex> lock(connect_mutex);
            joined = true;
        }
        connect_cv.notify_all();

        // Join completed, request to publish track
        send_add_track();
    }

    // Parse a TURN/STUN URL and add it to config as a proper rtc::IceServer struct.
    // libnice requires credentials passed as separate fields, not embedded in the URL.
    void add_ice_server(rtc::Configuration& config, const std::string& url,
                        const std::string& username, const std::string& credential) {
        // Determine scheme and strip it
        auto scheme_end = url.find(':');
        if (scheme_end == std::string::npos) return;
        std::string scheme = url.substr(0, scheme_end); // "stun", "turn", "turns"

        // Strip "//" if present
        size_t host_start = scheme_end + 1;
        if (url.size() > host_start + 1 && url[host_start] == '/' && url[host_start+1] == '/')
            host_start += 2;

        // Split transport query param (e.g. ?transport=tcp)
        std::string host_port_str = url.substr(host_start);
        auto query_pos = host_port_str.find('?');
        if (query_pos != std::string::npos)
            host_port_str = host_port_str.substr(0, query_pos);

        // Split host:port
        std::string hostname;
        uint16_t port = (scheme == "turns") ? 5349 : 3478;
        auto port_pos = host_port_str.rfind(':');
        if (port_pos != std::string::npos) {
            hostname = host_port_str.substr(0, port_pos);
            try { port = static_cast<uint16_t>(std::stoi(host_port_str.substr(port_pos + 1))); }
            catch (...) {}
        } else {
            hostname = host_port_str;
        }

        if (scheme == "stun") {
            std::cout << "[stoatpp voice] ICE STUN: " << hostname << ":" << port << "\n";
            config.iceServers.emplace_back(hostname, port);
        } else if ((scheme == "turn" || scheme == "turns") && !username.empty()) {
            // turns: = TURN over TLS (TCP), turn: = TURN over UDP
            auto relay = (scheme == "turns") ? rtc::IceServer::RelayType::TurnTls
                                              : rtc::IceServer::RelayType::TurnUdp;
            std::cout << "[stoatpp voice] ICE TURN (" << scheme << "): "
                      << hostname << ":" << port
                      << " user=" << username << "\n";
            config.iceServers.emplace_back(hostname, port, username, credential, relay);
        }
    }

    void initialize_peer_connections() {
        rtc::Configuration config;
        for (const auto& is : ice_servers) {
            for (const auto& url : is.urls) {
                add_ice_server(config, url, is.username, is.credential);
            }
        }

        std::cout << "[stoatpp voice] Creating Subscriber PeerConnection...\n";
        sub_pc = std::make_shared<rtc::PeerConnection>(config);
        sub_pc->onLocalDescription([this](rtc::Description description) {
            std::cout << "[stoatpp voice] Generated Subscriber Local Description\n";
            send_sdp_answer(std::string(description));
        });
        sub_pc->onLocalCandidate([this](rtc::Candidate candidate) {
            nlohmann::json cand_j;
            cand_j["candidate"] = std::string(candidate);
            cand_j["sdpMid"] = candidate.mid();
            cand_j["sdpMLineIndex"] = 0;
            send_trickle(cand_j.dump(), SignalTarget::SUBSCRIBER);
        });
        sub_pc->onStateChange([](rtc::PeerConnection::State state) {
            std::cout << "[stoatpp voice] Subscriber PC State: " << state << "\n";
        });

        std::cout << "[stoatpp voice] Creating Publisher PeerConnection...\n";
        pub_pc = std::make_shared<rtc::PeerConnection>(config);
        pub_pc->onLocalDescription([this](rtc::Description description) {
            std::cout << "[stoatpp voice] Generated Publisher Local Description\n";
            send_sdp_offer(std::string(description));
        });
        pub_pc->onLocalCandidate([this](rtc::Candidate candidate) {
            nlohmann::json cand_j;
            cand_j["candidate"] = std::string(candidate);
            cand_j["sdpMid"] = candidate.mid();
            cand_j["sdpMLineIndex"] = 0;
            send_trickle(cand_j.dump(), SignalTarget::PUBLISHER);
        });
        pub_pc->onStateChange([](rtc::PeerConnection::State state) {
            std::cout << "[stoatpp voice] Publisher PC State: " << state << "\n";
        });
    }

    void initialize_publisher_track() {
        std::cout << "[stoatpp voice] Initializing publisher audio track with SID: " << track_sid << "\n";
        uint32_t ssrc = 12345;
        uint8_t payloadType = 111;

        rtc::Description::Audio audio("audio", rtc::Description::Direction::SendOnly);
        audio.addOpusCodec(payloadType);
        // LiveKit expects the MSID's track ID to match the server-assigned track_sid
        audio.addSSRC(ssrc, "audio-send", "msid-label", track_sid);

        auto track = pub_pc->addTrack(audio);

        // Explicitly trigger offer generation — libnice's async GLib loop
        // may not fire onLocalDescription from addTrack alone in time.
        std::cout << "[stoatpp voice] Triggering publisher SDP offer...\n";
        pub_pc->setLocalDescription(rtc::Description::Type::Offer);

        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
            ssrc, "audio-send", payloadType, rtc::OpusRtpPacketizer::DefaultClockRate);
        auto packetizer = std::make_shared<rtc::OpusRtpPacketizer>(rtpConfig);
        track->setMediaHandler(packetizer);

        audio_track = track;

        // Initialize Opus encoder
        int error = 0;
        encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &error);
        if (error != OPUS_OK) {
            std::cout << "[stoatpp voice] Failed to create Opus encoder. Error: " << error << "\n";
        } else {
            opus_encoder_ctl(encoder, OPUS_SET_BITRATE(64000));
        }

        // Start send loop thread
        running = true;
        send_thread = std::thread(&impl::send_loop, this);
    }

    void send_loop() {
        const int frame_size = 960; // 20ms
        std::vector<int16_t> pcm_buf(frame_size);
        std::vector<uint8_t> opus_buf(4096);

        auto next_time = std::chrono::steady_clock::now();

        while (running) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this]() {
                    return pcm_queue.size() >= frame_size || !running;
                });

                if (!running) break;

                for (int i = 0; i < frame_size; ++i) {
                    pcm_buf[i] = pcm_queue.front();
                    pcm_queue.pop();
                }
            }

            if (audio_track && audio_track->isOpen() && encoder) {
                int encoded_bytes = opus_encode(encoder, pcm_buf.data(), frame_size, opus_buf.data(), opus_buf.size());
                if (encoded_bytes > 0) {
                    audio_track->send(reinterpret_cast<const std::byte*>(opus_buf.data()), encoded_bytes);
                }
            }

            next_time += std::chrono::milliseconds(20);
            std::this_thread::sleep_until(next_time);

            if (std::chrono::steady_clock::now() > next_time + std::chrono::milliseconds(100)) {
                next_time = std::chrono::steady_clock::now();
            }
        }
    }
};

voice_connection::voice_connection() : pimpl_(std::make_unique<impl>()) {}
voice_connection::~voice_connection() {
    disconnect();
}

bool voice_connection::connect(const std::string& ws_url, const std::string& token) {
    disconnect();

    pimpl_->joined = false;
    pimpl_->failed = false;
    pimpl_->ice_servers.clear();

    std::string rtc_url = ws_url + "/rtc?access_token=" + token + "&protocol=8&sdk=cpp";
    std::cout << "[stoatpp voice] Connecting to WSS signaling: " << rtc_url << "\n";
    pimpl_->ws.setUrl(rtc_url);

    pimpl_->ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "[stoatpp voice] WebSocket Connected! Handshaking JoinRequest...\n";
            pimpl_->send_wrapped_join_request();
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            if (msg->binary) {
                pimpl_->handle_binary_message(msg->str);
            }
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "[stoatpp voice] WebSocket Error: " << msg->errorInfo.reason << "\n";
            {
                std::lock_guard<std::mutex> lock(pimpl_->connect_mutex);
                pimpl_->failed = true;
            }
            pimpl_->connect_cv.notify_all();
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            std::cout << "[stoatpp voice] WebSocket Closed. Code: " << msg->closeInfo.code
                      << " | Reason: " << msg->closeInfo.reason << "\n";
            {
                std::lock_guard<std::mutex> lock(pimpl_->connect_mutex);
                pimpl_->failed = true;
            }
            pimpl_->connect_cv.notify_all();
            pimpl_->stop_thread();
        }
    });

    pimpl_->ws.disableAutomaticReconnection();
    pimpl_->ws.start();

    // Wait for JoinResponse to verify connection success
    std::unique_lock<std::mutex> lock(pimpl_->connect_mutex);
    if (pimpl_->connect_cv.wait_for(lock, std::chrono::seconds(10), [this]() { return pimpl_->joined || pimpl_->failed; })) {
        return pimpl_->joined;
    }
    std::cout << "[stoatpp voice] WebSocket signaling join timeout\n";
    disconnect();
    return false;
}

void voice_connection::disconnect() {
    pimpl_->stop_thread();
    pimpl_->ws.stop();
    pimpl_->sub_pc.reset();
    pimpl_->pub_pc.reset();
    pimpl_->audio_track.reset();
    if (pimpl_->encoder) {
        opus_encoder_destroy(pimpl_->encoder);
        pimpl_->encoder = nullptr;
    }
    pimpl_->joined = false;
    pimpl_->failed = false;
}

bool voice_connection::is_connected() const {
    return pimpl_->joined && pimpl_->pub_pc && pimpl_->pub_pc->state() == rtc::PeerConnection::State::Connected;
}

bool voice_connection::write_pcm(const int16_t* samples, size_t count) {
    if (!pimpl_->running) return false;
    std::lock_guard<std::mutex> lock(pimpl_->queue_mutex);
    for (size_t i = 0; i < count; ++i) {
        pimpl_->pcm_queue.push(samples[i]);
    }
    pimpl_->queue_cv.notify_one();
    return true;
}

} // namespace stoatpp
