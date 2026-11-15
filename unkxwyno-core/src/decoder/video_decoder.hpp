/**
 * @file video_decoder.hpp
 * @brief Abstract interface for OS-native hardware video decoders.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <functional>
#include <memory>
#include "../../../unkxwyno-protocol/include/unkxwyno_protocol.h"

namespace unkxwyno::decoder {

/**
 * @brief Decoded video frame descriptor.
 * For hardware decoding, native_surface holds the GPU handle (ID3D11Texture2D on Windows, CVPixelBufferRef on macOS).
 */
struct DecodedFrame {
    void* native_surface{nullptr};  // GPU texture or native handle
    const uint8_t* raw_pixels{nullptr}; // Optional software fallback buffer
    int width{0};
    int height{0};
    int stride{0};
    uint64_t pts_us{0};
    bool is_hardware_surface{true};
};

using FrameDecodedCallback = std::function<void(const DecodedFrame& frame)>;

/**
 * @brief Abstract hardware-accelerated video decoder.
 */
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    virtual bool Initialize(uint32_t width, uint32_t height, unkxwyno_video_codec_t codec) = 0;
    virtual void Shutdown() = 0;

    /**
     * @brief Feed an Annex-B H.264/HEVC NAL unit into the hardware decoder.
     */
    virtual bool DecodeNalu(std::span<const uint8_t> nalu, uint64_t pts_us) = 0;

    virtual void Flush() = 0;

    virtual void SetFrameCallback(FrameDecodedCallback callback) = 0;
};

} // namespace unkxwyno::decoder
