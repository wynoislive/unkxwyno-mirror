#pragma once

#include "unkxwyno/core/c_api.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace unkxwyno::core {

inline constexpr std::uint32_t kProtocolMajor = 1;
inline constexpr std::uint32_t kProtocolMinor = 0;

enum class ResolutionPreset : std::uint8_t {
    Auto,
    P480,
    Hd720,
    FullHd1080,
    Qhd1440,
    Uhd2160,
    Native,
    Custom,
};

enum class FrameRatePreset : std::uint16_t {
    Auto = 0,
    Fps24 = 24,
    Fps30 = 30,
    Fps45 = 45,
    Fps60 = 60,
    Fps90 = 90,
    Fps120 = 120,
};

enum class VideoCodec : std::uint8_t {
    H264 = 1,
    Hevc = 2,
    Av1 = 3,
};

enum class TransportKind : std::uint8_t {
    LocalNetwork = 1,
    AndroidAccessory = 2,
    AppleUsbMux = 3,
    UsbNetwork = 4,
    Loopback = 5,
};

enum class BitrateMode : std::uint8_t {
    Adaptive,
    Fixed,
};

enum class LatencyMode : std::uint8_t {
    Low,
    Balanced,
    Quality,
};

enum class NegotiationStatus : std::uint8_t {
    Supported,
    Unsupported,
    InvalidRequest,
    IncompatibleProtocol,
};

enum class UnsupportedReason : std::uint8_t {
    None,
    InvalidDimensions,
    NoCommonResolution,
    FrameRateUnavailable,
    NoCommonCodec,
    BitrateUnavailable,
    SenderUnavailable,
    ReceiverUnavailable,
    ProtocolVersionMismatch,
};

struct Dimensions {
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0 && height > 0;
    }

    [[nodiscard]] constexpr std::uint64_t pixels() const noexcept {
        return static_cast<std::uint64_t>(width) * height;
    }

    friend constexpr bool operator==(const Dimensions&, const Dimensions&) = default;
};

struct VideoModeCapability {
    Dimensions dimensions{};
    std::vector<std::uint16_t> frame_rates;
};

struct CodecCapability {
    VideoCodec codec{VideoCodec::H264};
    bool can_encode{};
    bool can_decode{};
    bool hardware_encode{};
    bool hardware_decode{};
};

struct EndpointCapabilities {
    std::uint32_t protocol_major{kProtocolMajor};
    std::uint32_t protocol_minor{kProtocolMinor};
    Dimensions native_dimensions{};
    std::vector<VideoModeCapability> send_modes;
    std::vector<VideoModeCapability> receive_modes;
    std::vector<CodecCapability> codecs;
    std::vector<TransportKind> transports;
    std::uint64_t max_send_bitrate_bps{};
    std::uint64_t max_receive_bitrate_bps{};
    std::uint16_t display_max_fps{};
    bool adaptive_bitrate{};
};

struct QualityRequest {
    ResolutionPreset resolution{ResolutionPreset::Auto};
    Dimensions custom_dimensions{};
    FrameRatePreset frame_rate{FrameRatePreset::Auto};
    std::vector<VideoCodec> codec_preference;
    BitrateMode bitrate_mode{BitrateMode::Adaptive};
    LatencyMode latency_mode{LatencyMode::Low};
    std::uint64_t requested_bitrate_bps{};
    std::uint64_t maximum_bitrate_bps{};
};

struct VideoProfile {
    Dimensions dimensions{};
    std::uint16_t frames_per_second{};
    VideoCodec codec{VideoCodec::H264};
    std::uint64_t bitrate_bps{};
    bool hardware_encode{};
    bool hardware_decode{};

    friend constexpr bool operator==(const VideoProfile&, const VideoProfile&) = default;
};

struct NegotiationResult {
    NegotiationStatus status{NegotiationStatus::InvalidRequest};
    UnsupportedReason reason{UnsupportedReason::None};
    std::string explanation;
    QualityRequest requested{};
    std::optional<VideoProfile> active;
    std::optional<VideoProfile> highest_alternative;
};

[[nodiscard]] UXM_API Dimensions dimensions_for_preset(
    ResolutionPreset preset,
    Dimensions native_dimensions,
    Dimensions custom_dimensions) noexcept;

[[nodiscard]] UXM_API NegotiationResult negotiate_quality(
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver,
    const QualityRequest& request);

}  // namespace unkxwyno::core

