#include "unkxwyno/core/types.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace unkxwyno::core {
namespace {

struct CommonMode {
    Dimensions dimensions{};
    std::vector<std::uint16_t> frame_rates;
};

const VideoModeCapability* find_mode(
    const std::vector<VideoModeCapability>& modes,
    Dimensions dimensions) {
    const auto it = std::find_if(modes.begin(), modes.end(), [dimensions](const auto& mode) {
        return mode.dimensions == dimensions;
    });
    return it == modes.end() ? nullptr : &*it;
}

const CodecCapability* find_codec(
    const std::vector<CodecCapability>& codecs,
    VideoCodec codec) {
    const auto it = std::find_if(codecs.begin(), codecs.end(), [codec](const auto& capability) {
        return capability.codec == codec;
    });
    return it == codecs.end() ? nullptr : &*it;
}

std::vector<CommonMode> common_modes(
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver) {
    std::vector<CommonMode> result;
    for (const auto& send_mode : sender.send_modes) {
        const auto* receive_mode = find_mode(receiver.receive_modes, send_mode.dimensions);
        if (receive_mode == nullptr || !send_mode.dimensions.valid()) {
            continue;
        }

        CommonMode common{};
        common.dimensions = send_mode.dimensions;
        for (const auto fps : send_mode.frame_rates) {
            const bool receiver_supports = std::find(
                receive_mode->frame_rates.begin(), receive_mode->frame_rates.end(), fps)
                != receive_mode->frame_rates.end();
            const bool display_supports = receiver.display_max_fps == 0 || fps <= receiver.display_max_fps;
            if (receiver_supports && display_supports) {
                common.frame_rates.push_back(fps);
            }
        }
        std::sort(common.frame_rates.begin(), common.frame_rates.end());
        common.frame_rates.erase(
            std::unique(common.frame_rates.begin(), common.frame_rates.end()),
            common.frame_rates.end());
        if (!common.frame_rates.empty()) {
            result.push_back(std::move(common));
        }
    }

    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.dimensions.pixels() != right.dimensions.pixels()) {
            return left.dimensions.pixels() > right.dimensions.pixels();
        }
        if (left.dimensions.width != right.dimensions.width) {
            return left.dimensions.width > right.dimensions.width;
        }
        return left.dimensions.height > right.dimensions.height;
    });
    return result;
}

std::vector<VideoCodec> requested_codecs(const QualityRequest& request) {
    if (!request.codec_preference.empty()) {
        return request.codec_preference;
    }
    return {VideoCodec::H264, VideoCodec::Hevc, VideoCodec::Av1};
}

std::optional<VideoCodec> choose_codec(
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver,
    const QualityRequest& request) {
    for (const auto codec : requested_codecs(request)) {
        const auto* send = find_codec(sender.codecs, codec);
        const auto* receive = find_codec(receiver.codecs, codec);
        if (send != nullptr && receive != nullptr && send->can_encode && receive->can_decode) {
            return codec;
        }
    }
    return std::nullopt;
}

std::uint64_t choose_bitrate(
    Dimensions dimensions,
    std::uint16_t fps,
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver,
    const QualityRequest& request) {
    std::uint64_t ceiling = std::min(sender.max_send_bitrate_bps, receiver.max_receive_bitrate_bps);
    if (request.maximum_bitrate_bps > 0) {
        ceiling = ceiling == 0
            ? request.maximum_bitrate_bps
            : std::min(ceiling, request.maximum_bitrate_bps);
    }
    if (ceiling == 0) {
        return 0;
    }
    if (request.bitrate_mode == BitrateMode::Fixed && request.requested_bitrate_bps > 0) {
        return request.requested_bitrate_bps <= ceiling ? request.requested_bitrate_bps : 0;
    }

    // A deterministic starting point, not a capability claim. Runtime adaptation
    // remains responsible for measuring congestion and delivered quality.
    const long double raw = static_cast<long double>(dimensions.pixels())
        * static_cast<long double>(fps) * 0.08L;
    const auto suggested = static_cast<std::uint64_t>(std::min<long double>(
        raw,
        static_cast<long double>(std::numeric_limits<std::uint64_t>::max())));
    return std::min(ceiling, std::max<std::uint64_t>(suggested, 500'000));
}

VideoProfile make_profile(
    const CommonMode& mode,
    std::uint16_t fps,
    VideoCodec codec,
    std::uint64_t bitrate,
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver) {
    const auto* send_codec = find_codec(sender.codecs, codec);
    const auto* receive_codec = find_codec(receiver.codecs, codec);
    return {
        .dimensions = mode.dimensions,
        .frames_per_second = fps,
        .codec = codec,
        .bitrate_bps = bitrate,
        .hardware_encode = send_codec != nullptr && send_codec->hardware_encode,
        .hardware_decode = receive_codec != nullptr && receive_codec->hardware_decode,
    };
}

std::optional<VideoProfile> best_alternative(
    const std::vector<CommonMode>& modes,
    VideoCodec codec,
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver,
    const QualityRequest& request) {
    for (const auto& mode : modes) {
        if (mode.frame_rates.empty()) {
            continue;
        }
        const auto fps = mode.frame_rates.back();
        const auto bitrate = choose_bitrate(mode.dimensions, fps, sender, receiver, request);
        if (bitrate > 0) {
            return make_profile(mode, fps, codec, bitrate, sender, receiver);
        }
    }
    return std::nullopt;
}

NegotiationResult unsupported(
    QualityRequest request,
    UnsupportedReason reason,
    std::string explanation,
    std::optional<VideoProfile> alternative = std::nullopt) {
    return {
        .status = NegotiationStatus::Unsupported,
        .reason = reason,
        .explanation = std::move(explanation),
        .requested = std::move(request),
        .active = std::nullopt,
        .highest_alternative = std::move(alternative),
    };
}

}  // namespace

Dimensions dimensions_for_preset(
    ResolutionPreset preset,
    Dimensions native_dimensions,
    Dimensions custom_dimensions) noexcept {
    switch (preset) {
        case ResolutionPreset::P480: return {854, 480};
        case ResolutionPreset::Hd720: return {1280, 720};
        case ResolutionPreset::FullHd1080: return {1920, 1080};
        case ResolutionPreset::Qhd1440: return {2560, 1440};
        case ResolutionPreset::Uhd2160: return {3840, 2160};
        case ResolutionPreset::Native: return native_dimensions;
        case ResolutionPreset::Custom: return custom_dimensions;
        case ResolutionPreset::Auto: return {};
    }
    return {};
}

NegotiationResult negotiate_quality(
    const EndpointCapabilities& sender,
    const EndpointCapabilities& receiver,
    const QualityRequest& request) {
    if (sender.protocol_major != receiver.protocol_major
        || sender.protocol_major != kProtocolMajor) {
        return {
            .status = NegotiationStatus::IncompatibleProtocol,
            .reason = UnsupportedReason::ProtocolVersionMismatch,
            .explanation = "Endpoints do not share protocol major version 1",
            .requested = request,
            .active = std::nullopt,
            .highest_alternative = std::nullopt,
        };
    }

    const auto modes = common_modes(sender, receiver);
    if (modes.empty()) {
        return unsupported(request, UnsupportedReason::NoCommonResolution,
            "Sender and receiver have no common video mode");
    }

    const auto codec = choose_codec(sender, receiver, request);
    if (!codec.has_value()) {
        return unsupported(request, UnsupportedReason::NoCommonCodec,
            "Sender cannot encode a codec that the receiver can decode");
    }

    const auto alternative = best_alternative(modes, *codec, sender, receiver, request);
    const CommonMode* selected_mode = nullptr;
    if (request.resolution == ResolutionPreset::Auto) {
        selected_mode = &modes.front();
    } else {
        const auto requested_dimensions = dimensions_for_preset(
            request.resolution, sender.native_dimensions, request.custom_dimensions);
        if (!requested_dimensions.valid()) {
            return {
                .status = NegotiationStatus::InvalidRequest,
                .reason = UnsupportedReason::InvalidDimensions,
                .explanation = "The requested dimensions are invalid",
                .requested = request,
                .active = std::nullopt,
                .highest_alternative = alternative,
            };
        }
        const auto it = std::find_if(modes.begin(), modes.end(), [requested_dimensions](const auto& mode) {
            return mode.dimensions == requested_dimensions;
        });
        if (it == modes.end()) {
            return unsupported(request, UnsupportedReason::NoCommonResolution,
                "The requested resolution is not supported by both endpoints", alternative);
        }
        selected_mode = &*it;
    }

    std::uint16_t fps = 0;
    if (request.frame_rate == FrameRatePreset::Auto) {
        fps = selected_mode->frame_rates.back();
    } else {
        fps = static_cast<std::uint16_t>(request.frame_rate);
        if (std::find(selected_mode->frame_rates.begin(), selected_mode->frame_rates.end(), fps)
            == selected_mode->frame_rates.end()) {
            return unsupported(request, UnsupportedReason::FrameRateUnavailable,
                "The requested frame rate is not supported at this resolution", alternative);
        }
    }

    const auto bitrate = choose_bitrate(
        selected_mode->dimensions, fps, sender, receiver, request);
    if (bitrate == 0) {
        return unsupported(request, UnsupportedReason::BitrateUnavailable,
            "The fixed bitrate exceeds an endpoint limit", alternative);
    }

    const auto active = make_profile(
        *selected_mode, fps, *codec, bitrate, sender, receiver);
    return {
        .status = NegotiationStatus::Supported,
        .reason = UnsupportedReason::None,
        .explanation = "Requested profile is supported",
        .requested = request,
        .active = active,
        .highest_alternative = active,
    };
}

}  // namespace unkxwyno::core

