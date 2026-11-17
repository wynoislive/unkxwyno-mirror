#pragma once

#include "unkxwyno/core/c_api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace unkxwyno::core {

inline constexpr std::uint32_t wire_magic = 0x55584D31U;  // "UXM1"
inline constexpr std::uint8_t wire_protocol_major = 1;
inline constexpr std::uint8_t wire_protocol_minor = 0;
inline constexpr std::size_t wire_header_size = 32;
static_assert(wire_header_size <= 255);

enum class WireChannel : std::uint8_t {
    Control = 1,
    Video = 2,
    Audio = 3,
};

enum class WireFlag : std::uint16_t {
    None = 0,
    KeyFrame = 1U << 0U,
    CodecConfig = 1U << 1U,
    EndOfMessage = 1U << 2U,
    Discontinuity = 1U << 3U,
};

[[nodiscard]] constexpr WireFlag operator|(WireFlag left, WireFlag right) noexcept {
    return static_cast<WireFlag>(
        static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr WireFlag operator&(WireFlag left, WireFlag right) noexcept {
    return static_cast<WireFlag>(
        static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
}

struct WireLimits {
    std::uint32_t max_control_payload = 64U * 1024U;
    std::uint32_t max_video_payload = 16U * 1024U * 1024U;
    std::uint32_t max_audio_payload = 256U * 1024U;
};

struct WireFrameMetadata {
    std::uint8_t protocol_major = wire_protocol_major;
    std::uint8_t protocol_minor = wire_protocol_minor;
    WireChannel channel = WireChannel::Control;
    WireFlag flags = WireFlag::None;
    std::uint64_t sequence = 0;
    std::uint64_t monotonic_timestamp_us = 0;

    friend bool operator==(const WireFrameMetadata&, const WireFrameMetadata&) = default;
};

struct WireFrame {
    WireFrameMetadata metadata;
    std::vector<std::uint8_t> payload;

    friend bool operator==(const WireFrame&, const WireFrame&) = default;
};

enum class WireStatus : std::uint8_t {
    Ok = 0,
    NeedMoreData,
    InvalidMagic,
    UnsupportedVersion,
    InvalidHeaderSize,
    InvalidChannel,
    InvalidFlags,
    InvalidReservedField,
    PayloadTooLarge,
};

struct WireEncodeResult {
    WireStatus status = WireStatus::Ok;
    std::vector<std::uint8_t> bytes;
};

struct WireDecodeResult {
    WireStatus status = WireStatus::NeedMoreData;
    std::size_t bytes_consumed = 0;
    std::optional<WireFrame> frame;
};

[[nodiscard]] UXM_API WireEncodeResult encode_wire_frame(
    const WireFrame& frame,
    const WireLimits& limits = {});

// Decodes at most one frame. Extra bytes are left for the caller and reported
// through bytes_consumed. NeedMoreData never consumes a partial frame.
[[nodiscard]] UXM_API WireDecodeResult decode_wire_frame(
    std::span<const std::uint8_t> bytes,
    const WireLimits& limits = {});

}  // namespace unkxwyno::core
