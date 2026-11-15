/**
 * @file airplay_server.hpp
 * @brief Zero-Dependency C++ AirPlay RTSP/RTP Receiver with usbmuxd & mDNS discovery.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <span>
#include <vector>
#include "../decoder/video_decoder.hpp"
#include "../audio/audio_renderer.hpp"

namespace unkxwyno::protocol {

using VideoFrameCallback = std::function<void(std::span<const uint8_t> nalu, uint64_t pts_us)>;
using AudioFrameCallback = std::function<void(std::span<const uint8_t> pcm_data)>;

class AirPlayServer {
public:
    AirPlayServer();
    ~AirPlayServer();

    bool Start(uint16_t rtsp_port = 7000);
    void Stop();

    void SetVideoCallback(VideoFrameCallback callback) {
        video_callback_ = std::move(callback);
    }

    void SetAudioCallback(AudioFrameCallback callback) {
        audio_callback_ = std::move(callback);
    }

    [[nodiscard]] bool IsConnected() const {
        return is_connected_.load();
    }

private:
    void MdnsThreadProc();
    void UsbmuxdThreadProc();
    void RtspServerThreadProc(uint16_t port);
    void RtpVideoThreadProc(uint16_t port);

    // Cryptography helpers (Curve25519, ChaCha20-Poly1305, AES-128-CTR)
    void DecryptPayload(uint8_t* data, size_t length, uint64_t packet_num);

    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_connected_{false};

    std::thread mdns_thread_;
    std::thread usbmuxd_thread_;
    std::thread rtsp_thread_;
    std::thread rtp_thread_;

    VideoFrameCallback video_callback_;
    AudioFrameCallback audio_callback_;

    uint8_t session_aes_key_[16]{};
    uint8_t session_aes_iv_[16]{};
    bool is_encrypted_{false};
};

} // namespace unkxwyno::protocol
