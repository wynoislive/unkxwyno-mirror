/**
 * @file wasapi_renderer.hpp
 * @brief Ultra-low latency Windows Audio Session API (WASAPI) Renderer.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include "audio_renderer.hpp"

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace unkxwyno::audio {

class WasapiAudioRenderer final : public IAudioRenderer {
public:
    WasapiAudioRenderer();
    ~WasapiAudioRenderer() override;

    bool Initialize(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample) override;
    void Shutdown() override;
    bool EnqueuePcm(std::span<const uint8_t> pcm_data) override;
    void Flush() override;
    void SetVolume(float volume) override;
    uint32_t GetBufferedLatencyMs() const override;

private:
    void PlaybackThreadProc();

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
    Microsoft::WRL::ComPtr<IAudioRenderClient> render_client_;

    uint32_t sample_rate_ = 48000;
    uint16_t channels_ = 2;
    uint16_t bits_per_sample_ = 16;
    uint32_t frame_size_bytes_ = 4;
    UINT32 buffer_frame_count_ = 0;
    bool is_float_format_ = false;
    uint16_t mix_channels_ = 2;
    uint32_t mix_sample_rate_ = 48000;

    std::atomic<bool> is_running_{false};
    std::atomic<float> volume_{1.0f};
    std::thread playback_thread_;

    // SPSC Ring Buffer for PCM Samples
    mutable std::mutex ring_mutex_;
    std::condition_variable ring_cv_;
    std::vector<uint8_t> ring_buffer_;
    size_t ring_read_pos_ = 0;
    size_t ring_write_pos_ = 0;
    size_t ring_capacity_ = 0;
    size_t ring_available_bytes_ = 0;
};

} // namespace unkxwyno::audio

#endif // _WIN32
