#include "unkxwyno/core/bounded_queue.h"
#include "unkxwyno/core/c_api.h"
#include "unkxwyno/core/protocol.h"
#include "unkxwyno/core/session.h"
#include "unkxwyno/core/stats.h"
#include "unkxwyno/core/types.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace unkxwyno::core;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

EndpointCapabilities sender_caps() {
    return {
        .native_dimensions = {3840, 2160},
        .send_modes = {
            {{1920, 1080}, {30, 60}},
            {{3840, 2160}, {30, 60, 120}},
        },
        .receive_modes = {{{1920, 1080}, {30, 60}}},
        .codecs = {
            {VideoCodec::H264, true, true, true, true},
            {VideoCodec::Hevc, true, true, true, true},
        },
        .transports = {},
        .max_send_bitrate_bps = 100'000'000,
        .max_receive_bitrate_bps = 40'000'000,
        .display_max_fps = 60,
        .adaptive_bitrate = true,
    };
}

EndpointCapabilities receiver_caps() {
    return {
        .native_dimensions = {3840, 2160},
        .send_modes = {{{1920, 1080}, {30, 60}}},
        .receive_modes = {
            {{1920, 1080}, {30, 60}},
            {{3840, 2160}, {30, 60, 120}},
        },
        .codecs = {
            {VideoCodec::H264, true, true, true, true},
            {VideoCodec::Hevc, false, false, false, false},
        },
        .transports = {},
        .max_send_bitrate_bps = 40'000'000,
        .max_receive_bitrate_bps = 100'000'000,
        .display_max_fps = 120,
        .adaptive_bitrate = true,
    };
}

void test_quality_negotiation() {
    auto sender = sender_caps();
    auto receiver = receiver_caps();
    QualityRequest request{
        .resolution = ResolutionPreset::Uhd2160,
        .frame_rate = FrameRatePreset::Fps120,
        .codec_preference = {VideoCodec::Hevc, VideoCodec::H264},
        .maximum_bitrate_bps = 80'000'000,
    };

    auto result = negotiate_quality(sender, receiver, request);
    check(result.status == NegotiationStatus::Supported, "4K120 should negotiate when every stage supports it");
    check(result.active.has_value(), "supported negotiation has an active profile");
    check(result.active->dimensions == Dimensions{3840, 2160}, "active dimensions remain 4K");
    check(result.active->frames_per_second == 120, "active FPS remains 120");
    check(result.active->codec == VideoCodec::H264, "codec negotiation falls back deterministically to H.264");
    check(result.requested.resolution == ResolutionPreset::Uhd2160, "requested quality remains separate from active quality");

    receiver.display_max_fps = 60;
    result = negotiate_quality(sender, receiver, request);
    check(result.status == NegotiationStatus::Unsupported, "4K120 is refused by a 60 Hz receiver");
    check(!result.active.has_value(), "manual unsupported request is not silently activated");
    check(result.highest_alternative.has_value(), "unsupported request returns an alternative");
    check(result.highest_alternative->frames_per_second == 60, "alternative is the highest common FPS");
}

void test_bounded_queue() {
    BoundedQueue<int> queue(2);
    check(!queue.push(1).dropped_item, "first queue item is retained");
    check(!queue.push(2).dropped_item, "second queue item is retained");
    const auto overflow = queue.push(3);
    check(overflow.accepted && overflow.dropped_item, "overflow drops the oldest item");
    check(queue.size() == 2, "queue never exceeds its capacity");
    check(queue.try_pop() == 2, "oldest stale item was removed");
    check(queue.try_pop() == 3, "newest item remains available");
}

void test_session_recovery() {
    SessionController session;
    check(session.transition(SessionEvent::StartDiscovery), "idle to discovery");
    check(session.transition(SessionEvent::PeerFound), "discovery to pairing");
    check(session.transition(SessionEvent::PairingAccepted), "pairing to connecting");
    check(session.transition(SessionEvent::TransportConnected), "connecting to negotiation");
    check(session.transition(SessionEvent::NegotiationAccepted, 1'000), "negotiation to streaming");
    session.poll(3'999);
    check(session.state() == SessionState::Streaming, "session survives within heartbeat threshold");
    session.poll(4'000);
    check(session.state() == SessionState::Recovering, "missed heartbeats enter recovery");
    check(session.next_retry_delay_ms() == 250, "first recovery attempt starts at the configured delay");
    check(session.transition(SessionEvent::TransportRestored, 4'100), "transport can be restored");
    check(session.transition(SessionEvent::NegotiationAccepted, 4'200), "restored transport renegotiates before streaming");
    check(session.state() == SessionState::Streaming, "session returns to streaming");
    check(!session.transition(SessionEvent::PeerFound), "invalid transitions are rejected");

    check(session.transition(SessionEvent::TransportLost, 5'000), "stream can enter recovery after an invalid event");
    session.poll(65'000);
    check(session.state() == SessionState::Failed, "recovery window expires deterministically");
    check(session.failure() == SessionFailure::RecoveryTimedOut, "timeout has a typed failure");
}

void test_stats() {
    StatsAccumulator stats;
    QualityRequest requested{};
    requested.resolution = ResolutionPreset::Uhd2160;
    requested.frame_rate = FrameRatePreset::Fps120;
    VideoProfile active{{1920, 1080}, 60, VideoCodec::H264, 12'000'000, true, true};
    stats.configure(requested, active, 1'000);
    stats.record_capture(60);
    stats.record_encode(120.0, 60);
    stats.record_decode(59);
    stats.record_presented_unique(58);
    stats.record_drop({.transport = 1, .render = 1});
    stats.record_transfer(1'500'000, 100'000);
    stats.set_latency(91.0, 4.0, -12.0);
    const auto snapshot = stats.snapshot(2'000);
    check(snapshot.requested.resolution == ResolutionPreset::Uhd2160, "stats preserve requested resolution");
    check(snapshot.active.dimensions == Dimensions{1920, 1080}, "stats report actual dimensions independently");
    check(snapshot.rates.presented_unique_fps == 58.0, "stats count unique presented frames");
    check(snapshot.estimated_goodput_bps == 12'000'000, "stats calculate measured sender goodput");
    check(snapshot.mean_encode_ms == 2.0, "stats calculate mean encode time");
}

void test_wire_protocol() {
    WireFrame frame{
        .metadata = {
            .protocol_major = wire_protocol_major,
            .protocol_minor = 7,
            .channel = WireChannel::Video,
            .flags = WireFlag::KeyFrame | WireFlag::EndOfMessage,
            .sequence = 0x0102030405060708ULL,
            .monotonic_timestamp_us = 0x1112131415161718ULL,
        },
        .payload = {0xA0, 0xB1, 0xC2, 0xD3},
    };

    const auto encoded = encode_wire_frame(frame);
    check(encoded.status == WireStatus::Ok, "valid wire frame encodes");
    check(encoded.bytes.size() == wire_header_size + frame.payload.size(),
        "wire frame has a fixed header and exact payload length");
    check(encoded.bytes[0] == 'U' && encoded.bytes[1] == 'X'
            && encoded.bytes[2] == 'M' && encoded.bytes[3] == '1',
        "wire magic is explicit ASCII");
    check(encoded.bytes[12] == 0x01 && encoded.bytes[13] == 0x02
            && encoded.bytes[18] == 0x07 && encoded.bytes[19] == 0x08,
        "sequence is encoded in network byte order");
    check(encoded.bytes[20] == 0x11 && encoded.bytes[27] == 0x18,
        "monotonic timestamp is encoded in network byte order");
    check(encoded.bytes[28] == 0 && encoded.bytes[29] == 0
            && encoded.bytes[30] == 0 && encoded.bytes[31] == 4,
        "payload length is encoded in network byte order");

    const auto decoded = decode_wire_frame(encoded.bytes);
    check(decoded.status == WireStatus::Ok && decoded.frame.has_value(),
        "valid wire frame decodes");
    check(decoded.bytes_consumed == encoded.bytes.size(),
        "decoder reports the exact frame boundary");
    check(decoded.frame == frame, "wire frame round-trips without host-endian assumptions");

    for (std::size_t size = 0; size < encoded.bytes.size(); ++size) {
        const auto partial = decode_wire_frame(
            std::span<const std::uint8_t>(encoded.bytes).first(size));
        check(partial.status == WireStatus::NeedMoreData,
            "every truncated frame fails closed as incomplete");
        check(partial.bytes_consumed == 0 && !partial.frame.has_value(),
            "an incomplete frame is never partially consumed or emitted");
    }

    auto two_frames = encoded.bytes;
    two_frames.insert(two_frames.end(), encoded.bytes.begin(), encoded.bytes.end());
    const auto first = decode_wire_frame(two_frames);
    check(first.status == WireStatus::Ok && first.bytes_consumed == encoded.bytes.size(),
        "stream decoder consumes one frame and leaves trailing bytes");

    auto malformed = encoded.bytes;
    malformed[0] ^= 0xFF;
    check(decode_wire_frame(malformed).status == WireStatus::InvalidMagic,
        "invalid magic is rejected");
    malformed = encoded.bytes;
    malformed[4] = static_cast<std::uint8_t>(wire_protocol_major + 1U);
    check(decode_wire_frame(malformed).status == WireStatus::UnsupportedVersion,
        "unknown protocol major is rejected");
    malformed = encoded.bytes;
    malformed[7] = static_cast<std::uint8_t>(wire_header_size - 1U);
    check(decode_wire_frame(malformed).status == WireStatus::InvalidHeaderSize,
        "unexpected header size is rejected");
    malformed = encoded.bytes;
    malformed[6] = 0xFF;
    check(decode_wire_frame(malformed).status == WireStatus::InvalidChannel,
        "unknown channel is rejected before payload allocation");
    malformed = encoded.bytes;
    malformed[8] = 0x80;
    check(decode_wire_frame(malformed).status == WireStatus::InvalidFlags,
        "unknown flags are rejected");
    malformed = encoded.bytes;
    malformed[10] = 1;
    check(decode_wire_frame(malformed).status == WireStatus::InvalidReservedField,
        "nonzero reserved fields are rejected");
    malformed = encoded.bytes;
    malformed[28] = 0x01;
    malformed[29] = 0x00;
    malformed[30] = 0x00;
    malformed[31] = 0x01;
    check(decode_wire_frame(malformed).status == WireStatus::PayloadTooLarge,
        "oversized video length is rejected from the header without allocation");

    auto invalid_flags = frame;
    invalid_flags.metadata.channel = WireChannel::Control;
    check(encode_wire_frame(invalid_flags).status == WireStatus::InvalidFlags,
        "video-only keyframe flag cannot be placed on a control frame");

    auto audio = frame;
    audio.metadata.channel = WireChannel::Audio;
    audio.metadata.flags = WireFlag::EndOfMessage;
    WireLimits tiny_limits;
    tiny_limits.max_audio_payload = 3;
    check(encode_wire_frame(audio, tiny_limits).status == WireStatus::PayloadTooLarge,
        "per-channel payload limits are enforced while encoding");
}

void test_c_api() {
    check(uxm_core_abi_version() == UXM_CORE_ABI_VERSION, "C ABI version is explicit");
    auto* session = uxm_session_create();
    check(session != nullptr, "C ABI creates a session");
    check(uxm_session_transition(session, UXM_EVENT_START_DISCOVERY, 0) == UXM_RESULT_OK,
        "C ABI drives session transitions");
    check(uxm_session_get_state(session) == UXM_SESSION_DISCOVERING,
        "C ABI reports session state");
    uxm_session_destroy(session);

    uxm_endpoint_capabilities sender{};
    sender.struct_size = sizeof(sender);
    sender.abi_version = UXM_CORE_ABI_VERSION;
    sender.protocol_major = 1;
    sender.protocol_minor = 0;
    sender.native_dimensions = {1920, 1080};
    sender.send_mode_count = 1;
    sender.send_modes[0].dimensions = {1920, 1080};
    sender.send_modes[0].frame_rates[0] = 60;
    sender.send_modes[0].frame_rate_count = 1;
    sender.codec_count = 1;
    sender.codecs[0].codec = UXM_CODEC_H264;
    sender.codecs[0].can_encode = 1;
    sender.codecs[0].hardware_encode = 1;
    sender.max_send_bitrate_bps = 20'000'000;

    uxm_endpoint_capabilities receiver{};
    receiver.struct_size = sizeof(receiver);
    receiver.abi_version = UXM_CORE_ABI_VERSION;
    receiver.protocol_major = 1;
    receiver.protocol_minor = 0;
    receiver.native_dimensions = {1920, 1080};
    receiver.receive_mode_count = 1;
    receiver.receive_modes[0].dimensions = {1920, 1080};
    receiver.receive_modes[0].frame_rates[0] = 60;
    receiver.receive_modes[0].frame_rate_count = 1;
    receiver.codec_count = 1;
    receiver.codecs[0].codec = UXM_CODEC_H264;
    receiver.codecs[0].can_decode = 1;
    receiver.codecs[0].hardware_decode = 1;
    receiver.max_receive_bitrate_bps = 20'000'000;
    receiver.display_max_fps = 60;

    uxm_quality_request request{};
    request.struct_size = sizeof(request);
    request.abi_version = UXM_CORE_ABI_VERSION;
    request.resolution_preset = UXM_RESOLUTION_FULL_HD;
    request.frames_per_second = 60;
    request.codec_preference[0] = UXM_CODEC_H264;
    request.codec_preference_count = 1;
    request.adaptive_bitrate = 1;
    request.maximum_bitrate_bps = 20'000'000;

    uxm_negotiation_result result{};
    result.struct_size = sizeof(result);
    result.abi_version = UXM_CORE_ABI_VERSION;
    check(uxm_negotiate_quality(&sender, &receiver, &request, &result) == UXM_RESULT_OK,
        "C ABI negotiates a valid shared profile");
    check(result.has_active_profile == 1, "C ABI returns an active profile");
    check(result.active_profile.dimensions.width == 1920
            && result.active_profile.dimensions.height == 1080,
        "C ABI preserves negotiated dimensions");
    check(result.active_profile.frames_per_second == 60,
        "C ABI preserves negotiated frame rate");

    check(uxm_negotiate_quality(nullptr, nullptr, nullptr, &result) == UXM_RESULT_INVALID_ARGUMENT,
        "C ABI validates null inputs before dereference");

    result.struct_size = sizeof(result) - 1;
    check(uxm_negotiate_quality(&sender, &receiver, &request, &result)
            == UXM_RESULT_INCOMPATIBLE_ABI,
        "C ABI rejects undersized output structures");
}

}  // namespace

int main() {
    test_quality_negotiation();
    test_bounded_queue();
    test_session_recovery();
    test_stats();
    test_wire_protocol();
    test_c_api();
    if (failures == 0) {
        std::cout << "All independent core tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " independent core test(s) failed\n";
    return EXIT_FAILURE;
}

