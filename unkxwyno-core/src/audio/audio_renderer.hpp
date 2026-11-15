/**
 * @file audio_renderer.hpp
 * @brief Abstract Low-Latency Audio Renderer Interface.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <span>

namespace unkxwyno::audio {

class IAudioRenderer {
public:
    virtual ~IAudioRenderer() = default;

    /**
     * @brief Initialize the audio output device with the target PCM specifications.
     * @param sample_rate Sampling rate in Hz (e.g. 48000).
     * @param channels Channel count (e.g. 2 for stereo).
     * @param bits_per_sample Quantization depth (e.g. 16 bit PCM).
     * @return true if initialized successfully, false otherwise.
     */
    virtual bool Initialize(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample) = 0;

    /**
     * @brief Stop playback and release all OS audio endpoints.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Submit raw PCM samples to the playback ring buffer.
     * @param pcm_data Interleaved PCM bytes.
     * @return true if enqueued successfully, false if buffer overrun.
     */
    virtual bool EnqueuePcm(std::span<const uint8_t> pcm_data) = 0;

    /**
     * @brief Flush internal buffers (e.g. on pause or seek).
     */
    virtual void Flush() = 0;

    /**
     * @brief Set playback volume (0.0f = mute, 1.0f = nominal).
     */
    virtual void SetVolume(float volume) = 0;

    /**
     * @brief Query current buffered audio latency in milliseconds.
     */
    virtual uint32_t GetBufferedLatencyMs() const = 0;

    /**
     * @brief Factory method for the platform's lowest-latency audio backend.
     */
    static std::unique_ptr<IAudioRenderer> Create();
};

} // namespace unkxwyno::audio
