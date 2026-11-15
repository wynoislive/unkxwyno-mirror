/**
 * @file wasapi_renderer.cpp
 * @brief Implementation of Low-Latency WASAPI Audio Renderer with Format Adaptation.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "wasapi_renderer.hpp"

#ifdef _WIN32
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <avrt.h>
#include <mmreg.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

namespace unkxwyno::audio {

using Microsoft::WRL::ComPtr;

std::unique_ptr<IAudioRenderer> IAudioRenderer::Create() {
    return std::make_unique<WasapiAudioRenderer>();
}

WasapiAudioRenderer::WasapiAudioRenderer() = default;

WasapiAudioRenderer::~WasapiAudioRenderer() {
    Shutdown();
}

bool WasapiAudioRenderer::Initialize(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample) {
    Shutdown();

    sample_rate_ = sample_rate;
    channels_ = channels;
    bits_per_sample_ = bits_per_sample;
    frame_size_bytes_ = (channels_ * bits_per_sample_) / 8;

    // Allocate ring buffer for up to 250ms of audio
    ring_capacity_ = (sample_rate_ * frame_size_bytes_ * 250) / 1000;
    ring_buffer_.resize(ring_capacity_, 0);
    ring_read_pos_ = 0;
    ring_write_pos_ = 0;
    ring_available_bytes_ = 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator_)
    );
    if (FAILED(hr)) {
        std::cerr << "[WASAPI] Failed to create MMDeviceEnumerator. hr=0x" << std::hex << hr << "\n";
        return false;
    }

    hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr)) {
        std::cerr << "[WASAPI] Failed to get default audio endpoint. hr=0x" << std::hex << hr << "\n";
        return false;
    }

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audio_client_);
    if (FAILED(hr)) {
        std::cerr << "[WASAPI] Failed to activate IAudioClient. hr=0x" << std::hex << hr << "\n";
        return false;
    }

    // Query mix format
    WAVEFORMATEX* mix_format = nullptr;
    hr = audio_client_->GetMixFormat(&mix_format);
    if (FAILED(hr) || !mix_format) {
        std::cerr << "[WASAPI] Failed to get mix format. hr=0x" << std::hex << hr << "\n";
        return false;
    }

    // Request 10ms shared buffer period (100,000 in 100ns units)
    REFERENCE_TIME requested_duration = 100000;
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        requested_duration,
        0,
        mix_format,
        nullptr
    );

    if (FAILED(hr)) {
        std::cerr << "[WASAPI] IAudioClient::Initialize failed. hr=0x" << std::hex << hr << "\n";
        CoTaskMemFree(mix_format);
        return false;
    }

    hr = audio_client_->GetBufferSize(&buffer_frame_count_);
    if (FAILED(hr)) {
        CoTaskMemFree(mix_format);
        return false;
    }

    hr = audio_client_->GetService(IID_PPV_ARGS(&render_client_));
    CoTaskMemFree(mix_format);
    if (FAILED(hr)) {
        std::cerr << "[WASAPI] Failed to get IAudioRenderClient. hr=0x" << std::hex << hr << "\n";
        return false;
    }

    is_running_ = true;
    playback_thread_ = std::thread(&WasapiAudioRenderer::PlaybackThreadProc, this);

    std::cout << "[WASAPI] Audio engine initialized. Buffer frames: " << buffer_frame_count_ 
              << " (" << (buffer_frame_count_ * 1000 / sample_rate_) << "ms latency window)\n";
    return true;
}

void WasapiAudioRenderer::Shutdown() {
    is_running_ = false;
    ring_cv_.notify_all();

    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    render_client_.Reset();
    audio_client_.Reset();
    device_.Reset();
    enumerator_.Reset();
}

bool WasapiAudioRenderer::EnqueuePcm(std::span<const uint8_t> pcm_data) {
    if (!is_running_ || pcm_data.empty()) return false;

    std::unique_lock<std::mutex> lock(ring_mutex_);

    // If buffer is over 100ms full, discard oldest samples to maintain low latency
    size_t max_allowed_bytes = (sample_rate_ * frame_size_bytes_ * 100) / 1000;
    if (ring_available_bytes_ > max_allowed_bytes) {
        size_t excess = ring_available_bytes_ - max_allowed_bytes;
        ring_read_pos_ = (ring_read_pos_ + excess) % ring_capacity_;
        ring_available_bytes_ -= excess;
    }

    // Write samples into circular buffer
    for (uint8_t byte : pcm_data) {
        if (ring_available_bytes_ < ring_capacity_) {
            ring_buffer_[ring_write_pos_] = byte;
            ring_write_pos_ = (ring_write_pos_ + 1) % ring_capacity_;
            ring_available_bytes_++;
        } else {
            // Buffer full, drop byte
            break;
        }
    }

    lock.unlock();
    ring_cv_.notify_one();
    return true;
}

void WasapiAudioRenderer::Flush() {
    std::lock_guard<std::mutex> lock(ring_mutex_);
    ring_read_pos_ = 0;
    ring_write_pos_ = 0;
    ring_available_bytes_ = 0;
}

void WasapiAudioRenderer::SetVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

uint32_t WasapiAudioRenderer::GetBufferedLatencyMs() const {
    std::lock_guard<std::mutex> lock(ring_mutex_);
    if (frame_size_bytes_ == 0 || sample_rate_ == 0) return 0;
    size_t samples = ring_available_bytes_ / frame_size_bytes_;
    return static_cast<uint32_t>((samples * 1000) / sample_rate_);
}

void WasapiAudioRenderer::PlaybackThreadProc() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    DWORD task_index = 0;
    HANDLE mmcss_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
    if (!mmcss_handle) {
        mmcss_handle = AvSetMmThreadCharacteristicsW(L"Audio", &task_index);
    }

    audio_client_->Start();

    while (is_running_) {
        UINT32 padding = 0;
        HRESULT hr = audio_client_->GetCurrentPadding(&padding);
        if (FAILED(hr)) break;

        UINT32 frames_needed = buffer_frame_count_ - padding;
        if (frames_needed > 0) {
            BYTE* buffer = nullptr;
            hr = render_client_->GetBuffer(frames_needed, &buffer);
            if (SUCCEEDED(hr)) {
                size_t bytes_to_read = frames_needed * frame_size_bytes_;
                std::unique_lock<std::mutex> lock(ring_mutex_);

                size_t bytes_available = ring_available_bytes_;
                size_t actual_bytes = std::min(bytes_to_read, bytes_available);

                // Read available PCM samples
                for (size_t i = 0; i < actual_bytes; ++i) {
                    buffer[i] = ring_buffer_[ring_read_pos_];
                    ring_read_pos_ = (ring_read_pos_ + 1) % ring_capacity_;
                }
                ring_available_bytes_ -= actual_bytes;
                lock.unlock();

                // If starved, pad remaining buffer with silence
                if (actual_bytes < bytes_to_read) {
                    std::memset(buffer + actual_bytes, 0, bytes_to_read - actual_bytes);
                }

                // Apply volume scaling if needed
                float vol = volume_.load();
                if (vol < 0.999f && bits_per_sample_ == 16) {
                    auto* samples16 = reinterpret_cast<int16_t*>(buffer);
                    size_t sample_count = bytes_to_read / sizeof(int16_t);
                    for (size_t s = 0; s < sample_count; ++s) {
                        samples16[s] = static_cast<int16_t>(samples16[s] * vol);
                    }
                }

                render_client_->ReleaseBuffer(frames_needed, 0);
            }
        }

        // Sleep for ~2.5ms (half the typical audio quantum)
        Sleep(2);
    }

    audio_client_->Stop();

    if (mmcss_handle) {
        AvRevertMmThreadCharacteristics(mmcss_handle);
    }

    CoUninitialize();
}

} // namespace unkxwyno::audio

#endif // _WIN32
