#include "unkxwyno/core/stats.h"

#include <algorithm>
#include <utility>

namespace unkxwyno::core {

void StatsAccumulator::configure(
    QualityRequest requested,
    VideoProfile active,
    std::uint64_t now_ms) {
    std::scoped_lock lock(mutex_);
    requested_ = std::move(requested);
    active_ = active;
    window_start_ms_ = now_ms;
    captured_ = encoded_ = decoded_ = presented_ = 0;
    drops_ = {};
    sent_ = received_ = 0;
    total_encode_ms_ = 0;
    latency_ms_ = latency_uncertainty_ms_ = av_offset_ms_ = 0;
}

void StatsAccumulator::record_capture(std::uint64_t count) {
    std::scoped_lock lock(mutex_);
    captured_ += count;
}

void StatsAccumulator::record_encode(double duration_ms, std::uint64_t count) {
    std::scoped_lock lock(mutex_);
    encoded_ += count;
    total_encode_ms_ += std::max(0.0, duration_ms);
}

void StatsAccumulator::record_decode(std::uint64_t count) {
    std::scoped_lock lock(mutex_);
    decoded_ += count;
}

void StatsAccumulator::record_presented_unique(std::uint64_t count) {
    std::scoped_lock lock(mutex_);
    presented_ += count;
}

void StatsAccumulator::record_drop(StageDrops drops) {
    std::scoped_lock lock(mutex_);
    drops_.capture += drops.capture;
    drops_.encode += drops.encode;
    drops_.transport += drops.transport;
    drops_.decode += drops.decode;
    drops_.render += drops.render;
}

void StatsAccumulator::record_transfer(
    std::uint64_t sent_bytes,
    std::uint64_t received_bytes) {
    std::scoped_lock lock(mutex_);
    sent_ += sent_bytes;
    received_ += received_bytes;
}

void StatsAccumulator::set_latency(
    double estimate_ms,
    double uncertainty_ms,
    double av_offset_ms) {
    std::scoped_lock lock(mutex_);
    latency_ms_ = std::max(0.0, estimate_ms);
    latency_uncertainty_ms_ = std::max(0.0, uncertainty_ms);
    av_offset_ms_ = av_offset_ms;
}

SessionStats StatsAccumulator::snapshot(std::uint64_t now_ms) const {
    std::scoped_lock lock(mutex_);
    const auto elapsed_ms = now_ms > window_start_ms_ ? now_ms - window_start_ms_ : 0;
    const auto seconds = elapsed_ms > 0 ? static_cast<double>(elapsed_ms) / 1'000.0 : 0.0;
    const auto rate = [seconds](std::uint64_t count) {
        return seconds > 0.0 ? static_cast<double>(count) / seconds : 0.0;
    };

    return {
        .requested = requested_,
        .active = active_,
        .rates = {
            .captured_fps = rate(captured_),
            .encoded_fps = rate(encoded_),
            .decoded_fps = rate(decoded_),
            .presented_unique_fps = rate(presented_),
        },
        .drops = drops_,
        .bytes_sent = sent_,
        .bytes_received = received_,
        .estimated_goodput_bps = seconds > 0.0
            ? static_cast<std::uint64_t>((static_cast<double>(sent_) * 8.0) / seconds)
            : 0,
        .mean_encode_ms = encoded_ > 0 ? total_encode_ms_ / static_cast<double>(encoded_) : 0.0,
        .estimated_capture_to_present_ms = latency_ms_,
        .latency_uncertainty_ms = latency_uncertainty_ms_,
        .audio_video_offset_ms = av_offset_ms_,
    };
}

}  // namespace unkxwyno::core

