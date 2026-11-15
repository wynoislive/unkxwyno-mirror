/**
 * @file demuxer.hpp
 * @brief High-throughput binary stream demultiplexer and packet dispatcher.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <functional>
#include "../../../unkxwyno-protocol/include/unkxwyno_protocol.h"

namespace unkxwyno::protocol {

using VideoFrameHandler = std::function<void(uint8_t packet_type, std::span<const uint8_t> payload, uint8_t seq)>;
using AudioSampleHandler = std::function<void(uint8_t packet_type, std::span<const uint8_t> payload, uint8_t seq)>;
using ControlHandler = std::function<void(uint8_t packet_type, std::span<const uint8_t> payload)>;

class ProtocolDemuxer {
public:
    ProtocolDemuxer();
    ~ProtocolDemuxer();

    void SetVideoHandler(VideoFrameHandler handler) { video_handler_ = std::move(handler); }
    void SetAudioHandler(AudioSampleHandler handler) { audio_handler_ = std::move(handler); }
    void SetControlHandler(ControlHandler handler) { control_handler_ = std::move(handler); }

    /**
     * @brief Feed incoming raw chunk from the USB bulk endpoint.
     * Parses 8-byte headers, reconstructs full frames, and dispatches callbacks.
     */
    void PushData(std::span<const uint8_t> incoming_chunk);

    /**
     * @brief Reset internal buffer state.
     */
    void Reset();

    [[nodiscard]] uint64_t GetTotalPacketsParsed() const { return total_packets_; }
    [[nodiscard]] uint64_t GetTotalBytesReceived() const { return total_bytes_; }
    [[nodiscard]] uint64_t GetDroppedPackets() const { return dropped_packets_; }

private:
    std::vector<uint8_t> accumulator_;
    VideoFrameHandler video_handler_;
    AudioSampleHandler audio_handler_;
    ControlHandler control_handler_;

    uint8_t expected_sequence_{0};
    bool has_first_sequence_{false};

    uint64_t total_packets_{0};
    uint64_t total_bytes_{0};
    uint64_t dropped_packets_{0};
};

} // namespace unkxwyno::protocol
