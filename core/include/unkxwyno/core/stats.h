#pragma once

#include "unkxwyno/core/types.h"

#include <cstdint>
#include <mutex>

namespace unkxwyno::core {

struct StageRates {
    double captured_fps{};
    double encoded_fps{};
    double decoded_fps{};
    double presented_unique_fps{};
};

struct StageDrops {
    std::uint64_t capture{};
    std::uint64_t encode{};
    std::uint64_t transport{};
    std::uint64_t decode{};
    std::uint64_t render{};
};

struct SessionStats {
    QualityRequest requested{};
    VideoProfile active{};
    StageRates rates{};
    StageDrops drops{};
    std::uint64_t bytes_sent{};
    std::uint64_t bytes_received{};
    std::uint64_t estimated_goodput_bps{};
    double mean_encode_ms{};
    double estimated_capture_to_present_ms{};
    double latency_uncertainty_ms{};
    double audio_video_offset_ms{};
};

class UXM_API StatsAccumulator final {
public:
    void configure(QualityRequest requested, VideoProfile active, std::uint64_t now_ms);
    void record_capture(std::uint64_t count = 1);
    void record_encode(double duration_ms, std::uint64_t count = 1);
    void record_decode(std::uint64_t count = 1);
    void record_presented_unique(std::uint64_t count = 1);
    void record_drop(StageDrops drops);
    void record_transfer(std::uint64_t sent_bytes, std::uint64_t received_bytes);
    void set_latency(double estimate_ms, double uncertainty_ms, double av_offset_ms);
    [[nodiscard]] SessionStats snapshot(std::uint64_t now_ms) const;

private:
    mutable std::mutex mutex_;
    QualityRequest requested_{};
    VideoProfile active_{};
    std::uint64_t window_start_ms_{};
    std::uint64_t captured_{};
    std::uint64_t encoded_{};
    std::uint64_t decoded_{};
    std::uint64_t presented_{};
    StageDrops drops_{};
    std::uint64_t sent_{};
    std::uint64_t received_{};
    double total_encode_ms_{};
    double latency_ms_{};
    double latency_uncertainty_ms_{};
    double av_offset_ms_{};
};

}  // namespace unkxwyno::core

