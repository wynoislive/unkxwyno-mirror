#include "unkxwyno/core/protocol.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace unkxwyno::core {
namespace {

constexpr std::uint16_t known_flag_mask =
    static_cast<std::uint16_t>(WireFlag::KeyFrame)
    | static_cast<std::uint16_t>(WireFlag::CodecConfig)
    | static_cast<std::uint16_t>(WireFlag::EndOfMessage)
    | static_cast<std::uint16_t>(WireFlag::Discontinuity);

[[nodiscard]] bool is_valid_channel(WireChannel channel) noexcept {
    switch (channel) {
    case WireChannel::Control:
    case WireChannel::Video:
    case WireChannel::Audio:
        return true;
    }
    return false;
}

[[nodiscard]] bool has_flag(WireFlag flags, WireFlag value) noexcept {
    return (static_cast<std::uint16_t>(flags) & static_cast<std::uint16_t>(value)) != 0;
}

[[nodiscard]] WireStatus validate_metadata(const WireFrameMetadata& metadata) noexcept {
    if (metadata.protocol_major != wire_protocol_major) {
        return WireStatus::UnsupportedVersion;
    }
    if (!is_valid_channel(metadata.channel)) {
        return WireStatus::InvalidChannel;
    }
    const auto flags = static_cast<std::uint16_t>(metadata.flags);
    if ((flags & static_cast<std::uint16_t>(~known_flag_mask)) != 0U) {
        return WireStatus::InvalidFlags;
    }
    if (metadata.channel != WireChannel::Video && has_flag(metadata.flags, WireFlag::KeyFrame)) {
        return WireStatus::InvalidFlags;
    }
    return WireStatus::Ok;
}

[[nodiscard]] std::uint32_t payload_limit(WireChannel channel, const WireLimits& limits) noexcept {
    switch (channel) {
    case WireChannel::Control:
        return limits.max_control_payload;
    case WireChannel::Video:
        return limits.max_video_payload;
    case WireChannel::Audio:
        return limits.max_audio_payload;
    }
    return 0;
}

void write_u16(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value) noexcept {
    bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

void write_u32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) noexcept {
    bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16U);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

void write_u64(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        const auto shift = static_cast<unsigned>((sizeof(value) - 1U - index) * 8U);
        bytes[offset + index] = static_cast<std::uint8_t>(value >> shift);
    }
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | static_cast<std::uint16_t>(bytes[offset + 1]));
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3]);
}

[[nodiscard]] std::uint64_t read_u64(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8U) | static_cast<std::uint64_t>(bytes[offset + index]);
    }
    return value;
}

}  // namespace

WireEncodeResult encode_wire_frame(const WireFrame& frame, const WireLimits& limits) {
    WireEncodeResult result;
    result.status = validate_metadata(frame.metadata);
    if (result.status != WireStatus::Ok) {
        return result;
    }
    const auto limit = payload_limit(frame.metadata.channel, limits);
    if (frame.payload.size() > limit
        || frame.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.status = WireStatus::PayloadTooLarge;
        return result;
    }

    result.bytes.resize(wire_header_size + frame.payload.size());
    auto header = std::span<std::uint8_t>(result.bytes).first(wire_header_size);
    write_u32(header, 0, wire_magic);
    header[4] = frame.metadata.protocol_major;
    header[5] = frame.metadata.protocol_minor;
    header[6] = static_cast<std::uint8_t>(frame.metadata.channel);
    header[7] = static_cast<std::uint8_t>(wire_header_size);
    write_u16(header, 8, static_cast<std::uint16_t>(frame.metadata.flags));
    write_u16(header, 10, 0);
    write_u64(header, 12, frame.metadata.sequence);
    write_u64(header, 20, frame.metadata.monotonic_timestamp_us);
    write_u32(header, 28, static_cast<std::uint32_t>(frame.payload.size()));
    std::ranges::copy(frame.payload, result.bytes.begin() + static_cast<std::ptrdiff_t>(wire_header_size));
    return result;
}

WireDecodeResult decode_wire_frame(std::span<const std::uint8_t> bytes, const WireLimits& limits) {
    WireDecodeResult result;
    if (bytes.size() < wire_header_size) {
        return result;
    }
    if (read_u32(bytes, 0) != wire_magic) {
        result.status = WireStatus::InvalidMagic;
        return result;
    }

    WireFrame frame;
    frame.metadata.protocol_major = bytes[4];
    frame.metadata.protocol_minor = bytes[5];
    frame.metadata.channel = static_cast<WireChannel>(bytes[6]);
    if (bytes[7] != wire_header_size) {
        result.status = WireStatus::InvalidHeaderSize;
        return result;
    }
    frame.metadata.flags = static_cast<WireFlag>(read_u16(bytes, 8));
    if (read_u16(bytes, 10) != 0U) {
        result.status = WireStatus::InvalidReservedField;
        return result;
    }
    result.status = validate_metadata(frame.metadata);
    if (result.status != WireStatus::Ok) {
        return result;
    }

    frame.metadata.sequence = read_u64(bytes, 12);
    frame.metadata.monotonic_timestamp_us = read_u64(bytes, 20);
    const auto payload_size = read_u32(bytes, 28);
    if (payload_size > payload_limit(frame.metadata.channel, limits)) {
        result.status = WireStatus::PayloadTooLarge;
        return result;
    }

    const auto available_payload = bytes.size() - wire_header_size;
    if (payload_size > available_payload) {
        result.status = WireStatus::NeedMoreData;
        return result;
    }

    const auto frame_size = wire_header_size + static_cast<std::size_t>(payload_size);
    frame.payload.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(wire_header_size),
        bytes.begin() + static_cast<std::ptrdiff_t>(frame_size));
    result.status = WireStatus::Ok;
    result.bytes_consumed = frame_size;
    result.frame = std::move(frame);
    return result;
}

}  // namespace unkxwyno::core
