#include "unkxwyno/core/c_api.h"

#include "unkxwyno/core/session.h"
#include "unkxwyno/core/types.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <utility>
#include <vector>

using unkxwyno::core::BitrateMode;
using unkxwyno::core::CodecCapability;
using unkxwyno::core::Dimensions;
using unkxwyno::core::EndpointCapabilities;
using unkxwyno::core::FrameRatePreset;
using unkxwyno::core::NegotiationStatus;
using unkxwyno::core::QualityRequest;
using unkxwyno::core::ResolutionPreset;
using unkxwyno::core::SessionController;
using unkxwyno::core::SessionEvent;
using unkxwyno::core::VideoCodec;
using unkxwyno::core::VideoModeCapability;
using unkxwyno::core::VideoProfile;

struct uxm_session {
    SessionController controller{};
};

namespace {

bool valid_header(std::uint32_t size, std::size_t expected, std::uint32_t version) {
    return static_cast<std::size_t>(size) >= expected && version == UXM_CORE_ABI_VERSION;
}

std::optional<VideoCodec> to_codec(std::uint32_t value) {
    switch (value) {
        case UXM_CODEC_H264: return VideoCodec::H264;
        case UXM_CODEC_HEVC: return VideoCodec::Hevc;
        case UXM_CODEC_AV1: return VideoCodec::Av1;
        default: return std::nullopt;
    }
}

std::optional<ResolutionPreset> to_resolution(std::uint32_t value) {
    if (value > UXM_RESOLUTION_CUSTOM) {
        return std::nullopt;
    }
    return static_cast<ResolutionPreset>(value);
}

Dimensions to_dimensions(uxm_dimensions value) {
    return {.width = value.width, .height = value.height};
}

uxm_dimensions from_dimensions(Dimensions value) {
    return {.width = value.width, .height = value.height};
}

bool convert_endpoint(const uxm_endpoint_capabilities& source, EndpointCapabilities& target) {
    if (!valid_header(source.struct_size, sizeof(source), source.abi_version)
        || source.send_mode_count > UXM_MAX_VIDEO_MODES
        || source.receive_mode_count > UXM_MAX_VIDEO_MODES
        || source.codec_count > UXM_MAX_CODECS) {
        return false;
    }

    target.protocol_major = source.protocol_major;
    target.protocol_minor = source.protocol_minor;
    target.native_dimensions = to_dimensions(source.native_dimensions);
    target.max_send_bitrate_bps = source.max_send_bitrate_bps;
    target.max_receive_bitrate_bps = source.max_receive_bitrate_bps;
    target.display_max_fps = source.display_max_fps;
    target.adaptive_bitrate = source.adaptive_bitrate != 0;

    const auto convert_modes = [](const uxm_video_mode* source_modes,
                                  std::uint32_t count,
                                  std::vector<VideoModeCapability>& target_modes) {
        for (std::uint32_t i = 0; i < count; ++i) {
            if (source_modes[i].frame_rate_count > 7) {
                return false;
            }
            VideoModeCapability mode{};
            mode.dimensions = to_dimensions(source_modes[i].dimensions);
            for (std::uint32_t fps = 0; fps < source_modes[i].frame_rate_count; ++fps) {
                mode.frame_rates.push_back(source_modes[i].frame_rates[fps]);
            }
            target_modes.push_back(std::move(mode));
        }
        return true;
    };

    if (!convert_modes(source.send_modes, source.send_mode_count, target.send_modes)
        || !convert_modes(source.receive_modes, source.receive_mode_count, target.receive_modes)) {
        return false;
    }

    for (std::uint32_t i = 0; i < source.codec_count; ++i) {
        const auto codec = to_codec(source.codecs[i].codec);
        if (!codec.has_value()) {
            return false;
        }
        target.codecs.push_back({
            .codec = *codec,
            .can_encode = source.codecs[i].can_encode != 0,
            .can_decode = source.codecs[i].can_decode != 0,
            .hardware_encode = source.codecs[i].hardware_encode != 0,
            .hardware_decode = source.codecs[i].hardware_decode != 0,
        });
    }
    return true;
}

bool convert_request(const uxm_quality_request& source, QualityRequest& target) {
    if (!valid_header(source.struct_size, sizeof(source), source.abi_version)
        || source.codec_preference_count > UXM_MAX_CODECS) {
        return false;
    }
    const auto resolution = to_resolution(source.resolution_preset);
    if (!resolution.has_value()) {
        return false;
    }
    target.resolution = *resolution;
    target.custom_dimensions = to_dimensions(source.custom_dimensions);
    target.frame_rate = static_cast<FrameRatePreset>(source.frames_per_second);
    target.bitrate_mode = source.adaptive_bitrate != 0
        ? BitrateMode::Adaptive
        : BitrateMode::Fixed;
    target.requested_bitrate_bps = source.requested_bitrate_bps;
    target.maximum_bitrate_bps = source.maximum_bitrate_bps;
    for (std::uint32_t i = 0; i < source.codec_preference_count; ++i) {
        const auto codec = to_codec(source.codec_preference[i]);
        if (!codec.has_value()) {
            return false;
        }
        target.codec_preference.push_back(*codec);
    }
    return true;
}

uxm_video_profile from_profile(const VideoProfile& source) {
    return {
        .dimensions = from_dimensions(source.dimensions),
        .frames_per_second = source.frames_per_second,
        .codec = static_cast<std::uint32_t>(source.codec),
        .bitrate_bps = source.bitrate_bps,
        .hardware_encode = static_cast<std::uint8_t>(source.hardware_encode),
        .hardware_decode = static_cast<std::uint8_t>(source.hardware_decode),
    };
}

}  // namespace

extern "C" {

std::uint32_t uxm_core_abi_version(void) {
    return UXM_CORE_ABI_VERSION;
}

const char* uxm_core_version_string(void) {
    return "0.1.0";
}

uxm_result uxm_negotiate_quality(
    const uxm_endpoint_capabilities* sender,
    const uxm_endpoint_capabilities* receiver,
    const uxm_quality_request* request,
    uxm_negotiation_result* result) {
    if (sender == nullptr || receiver == nullptr || request == nullptr || result == nullptr) {
        return UXM_RESULT_INVALID_ARGUMENT;
    }
    if (!valid_header(result->struct_size, sizeof(*result), result->abi_version)) {
        return UXM_RESULT_INCOMPATIBLE_ABI;
    }

    EndpointCapabilities cpp_sender;
    EndpointCapabilities cpp_receiver;
    QualityRequest cpp_request;
    if (!convert_endpoint(*sender, cpp_sender)
        || !convert_endpoint(*receiver, cpp_receiver)
        || !convert_request(*request, cpp_request)) {
        return UXM_RESULT_INCOMPATIBLE_ABI;
    }

    const auto negotiated = unkxwyno::core::negotiate_quality(
        cpp_sender, cpp_receiver, cpp_request);
    result->status = static_cast<std::uint32_t>(negotiated.status);
    result->reason = static_cast<std::uint32_t>(negotiated.reason);
    result->has_active_profile = static_cast<std::uint8_t>(negotiated.active.has_value());
    result->has_highest_alternative = static_cast<std::uint8_t>(
        negotiated.highest_alternative.has_value());
    result->active_profile = negotiated.active.has_value()
        ? from_profile(*negotiated.active)
        : uxm_video_profile{};
    result->highest_alternative = negotiated.highest_alternative.has_value()
        ? from_profile(*negotiated.highest_alternative)
        : uxm_video_profile{};

    return negotiated.status == NegotiationStatus::Supported
        ? UXM_RESULT_OK
        : UXM_RESULT_UNSUPPORTED;
}

uxm_session* uxm_session_create(void) {
    return new (std::nothrow) uxm_session{};
}

void uxm_session_destroy(uxm_session* session) {
    delete session;
}

uxm_result uxm_session_transition(
    uxm_session* session,
    std::uint32_t event,
    std::uint64_t now_ms) {
    if (session == nullptr || event > UXM_EVENT_FAIL) {
        return UXM_RESULT_INVALID_ARGUMENT;
    }
    return session->controller.transition(static_cast<SessionEvent>(event), now_ms)
        ? UXM_RESULT_OK
        : UXM_RESULT_INVALID_TRANSITION;
}

std::uint32_t uxm_session_get_state(const uxm_session* session) {
    return session == nullptr
        ? UXM_SESSION_FAILED
        : static_cast<std::uint32_t>(session->controller.state());
}

void uxm_session_observe_heartbeat(uxm_session* session, std::uint64_t now_ms) {
    if (session != nullptr) {
        session->controller.observe_heartbeat(now_ms);
    }
}

void uxm_session_poll(uxm_session* session, std::uint64_t now_ms) {
    if (session != nullptr) {
        session->controller.poll(now_ms);
    }
}

std::uint64_t uxm_session_next_retry_delay_ms(const uxm_session* session) {
    return session == nullptr ? 0 : session->controller.next_retry_delay_ms();
}

}  // extern "C"

