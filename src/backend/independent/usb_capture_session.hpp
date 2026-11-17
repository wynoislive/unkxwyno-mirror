#pragma once
#include "usb_capture_protocol.hpp"
#include <string>

namespace unkxwyno::usb {
struct CapturedVideo {
    std::vector<std::uint8_t> annex_b;
    std::uint32_t width{}, height{};
    std::int64_t time_100ns{};
    bool timestamp_valid{};
};
struct CaptureActions {
    std::vector<std::vector<std::uint8_t>> outgoing;
    std::optional<CapturedVideo> video;
    std::vector<std::uint8_t> pcm;
    std::uint32_t audio_rate{48000}, audio_channels{2}, audio_bits{16};
    std::string event;
};
// Stateful QuickTime USB session. All packet and nested atom lengths are checked
// before access; no proprietary code, credentials, or response blobs are used.
class UXM_API UsbCaptureSession {
public:
    explicit UsbCaptureSession(std::uint32_t display_width = 3840, std::uint32_t display_height = 2160);
    CaptureActions receive(const CapturePacket& packet);
    std::vector<std::uint8_t> video_need() const;
    std::vector<std::vector<std::uint8_t>> shutdown_packets() const;
private:
    std::uint64_t audio_clock_{}, video_clock_{};
    std::uint32_t display_width_, display_height_, width_{}, height_{}, nalu_length_{4};
    std::uint32_t audio_rate_{48000}, audio_channels_{2}, audio_bits_{16};
    std::vector<std::uint8_t> parameter_sets_;
    bool announced_{}, format_sent_{};
    std::int64_t frame_index_{};
};
}
