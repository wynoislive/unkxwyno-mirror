/**
 * @file unkxwyno_core.hpp
 * @brief Main engine class for unkXwyno Mirror Core.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include "../src/protocol/demuxer.hpp"
#include "../src/protocol/aoa2_client.hpp"
#include "../src/protocol/usbmuxd_client.hpp"
#include "../src/protocol/airplay_server.hpp"
#include "../src/network/tcp_server.hpp"
#include "../src/usb/usb_transport.hpp"
#include "../src/decoder/video_decoder.hpp"
#include "../src/renderer/video_renderer.hpp"
#include "../src/renderer/native_viewport.hpp"
#include "../src/audio/audio_renderer.hpp"
#include "../../unkxwyno-protocol/include/unkxwyno_abi.h"

namespace unkxwyno {

class Engine {
public:
    explicit Engine(const unkxwyno_config_t& config);
    ~Engine();

    unkxwyno_status_t SetCallbacks(unkxwyno_state_cb state_cb,
                                   unkxwyno_metrics_cb metrics_cb,
                                   unkxwyno_log_cb log_cb,
                                   void* user_data);

    unkxwyno_status_t Start();
    unkxwyno_status_t Stop();
    unkxwyno_status_t GetMetrics(unkxwyno_metrics_t* out_metrics);

    unkxwyno_status_t ViewportCreate(const char* title, int width, int height);
    unkxwyno_status_t ViewportShow(bool visible);
    unkxwyno_status_t ViewportSetFullscreen(bool fullscreen);
    unkxwyno_status_t ViewportGetBounds(int* out_x, int* out_y, int* out_width, int* out_height);
    void* ViewportGetNativeHandle();

    unkxwyno_status_t SetKeymapHud(bool visible, const unkxwyno_keymap_badge_t* badges, size_t count);
    unkxwyno_status_t SetScreenPower(bool power_on);

    unkxwyno_status_t InjectTouch(uint8_t pointer_id, uint8_t action, uint16_t x, uint16_t y, uint16_t pressure);
    unkxwyno_status_t InjectKey(uint16_t key_code, bool is_down, uint8_t modifiers);
    unkxwyno_status_t InjectMouse(int16_t delta_x, int16_t delta_y, int8_t wheel_v, int8_t wheel_h, uint8_t buttons);

private:
    void StreamingLoop();
    void Log(unkxwyno_log_level_t level, const std::string& msg);

    unkxwyno_config_t config_{};
    unkxwyno_state_cb state_cb_{nullptr};
    unkxwyno_metrics_cb metrics_cb_{nullptr};
    unkxwyno_log_cb log_cb_{nullptr};
    void* user_data_{nullptr};

    std::shared_ptr<usb::IUsbTransport> usb_transport_;
    std::shared_ptr<usb::IUsbDeviceSession> active_usb_session_;
    std::unique_ptr<protocol::Aoa2Client> aoa_client_;
    std::unique_ptr<protocol::UsbmuxdClient> usbmuxd_client_;
    std::unique_ptr<protocol::AirPlayServer> airplay_server_;
    std::unique_ptr<network::TcpServer> tcp_server_;
    protocol::ProtocolDemuxer demuxer_;

    std::unique_ptr<decoder::IVideoDecoder> video_decoder_;
    std::unique_ptr<renderer::IVideoRenderer> video_renderer_;
    std::unique_ptr<renderer::INativeViewport> viewport_;
    std::unique_ptr<audio::IAudioRenderer> audio_renderer_;

    std::atomic<bool> is_running_{false};
    std::thread streaming_thread_;
    std::mutex engine_mutex_;
};

} // namespace unkxwyno
