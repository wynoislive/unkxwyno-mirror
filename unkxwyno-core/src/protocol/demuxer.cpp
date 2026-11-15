/**
 * @file demuxer.cpp
 * @brief Implementation of ProtocolDemuxer.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "demuxer.hpp"
#include <cstring>
#include <iostream>

namespace unkxwyno::protocol {

ProtocolDemuxer::ProtocolDemuxer() {
    accumulator_.reserve(1024 * 1024); // Pre-allocate 1 MiB accumulator
}

ProtocolDemuxer::~ProtocolDemuxer() = default;

void ProtocolDemuxer::Reset() {
    accumulator_.clear();
    has_first_sequence_ = false;
    expected_sequence_  = 0;
}

void ProtocolDemuxer::PushData(std::span<const uint8_t> incoming_chunk) {
    if (incoming_chunk.empty()) return;

    total_bytes_ += incoming_chunk.size();
    accumulator_.insert(accumulator_.end(), incoming_chunk.begin(), incoming_chunk.end());

    size_t processed_offset = 0;

    // Loop while accumulator has at least a full header
    while (accumulator_.size() - processed_offset >= UNKXWYNO_PACKET_HEADER_SIZE) {
        const auto* raw_ptr = accumulator_.data() + processed_offset;
        const auto* hdr = reinterpret_cast<const unkxwyno_packet_header_t*>(raw_ptr);

        // Sanity check header
        if (!unkxwyno_header_is_valid(hdr)) {
            // Corrupt or misaligned stream: advance 1 byte to seek next valid header
            processed_offset += 1;
            continue;
        }

        uint32_t body_len = unkxwyno_to_le32(hdr->body_len);
        size_t total_packet_size = UNKXWYNO_PACKET_HEADER_SIZE + body_len;

        // Check if full packet has arrived
        if (accumulator_.size() - processed_offset < total_packet_size) {
            // Incomplete packet: await more chunk data
            break;
        }

        // Sequence drop detection
        if (has_first_sequence_) {
            uint8_t diff = static_cast<uint8_t>(hdr->sequence - expected_sequence_);
            if (diff > 0) {
                dropped_packets_ += diff;
            }
        } else {
            has_first_sequence_ = true;
        }
        expected_sequence_ = static_cast<uint8_t>(hdr->sequence + 1);
        total_packets_++;

        // Extract payload span
        std::span<const uint8_t> payload(raw_ptr + UNKXWYNO_PACKET_HEADER_SIZE, body_len);

        // Dispatch based on Channel ID
        switch (hdr->channel_id) {
            case UNKXWYNO_CHANNEL_VIDEO:
                if (video_handler_) {
                    video_handler_(hdr->packet_type, payload, hdr->sequence);
                }
                break;

            case UNKXWYNO_CHANNEL_AUDIO:
                if (audio_handler_) {
                    audio_handler_(hdr->packet_type, payload, hdr->sequence);
                }
                break;

            case UNKXWYNO_CHANNEL_CONTROL:
                if (control_handler_) {
                    control_handler_(hdr->packet_type, payload);
                }
                break;

            default:
                break;
        }

        processed_offset += total_packet_size;
    }

    // Compact accumulator
    if (processed_offset > 0) {
        accumulator_.erase(accumulator_.begin(), accumulator_.begin() + processed_offset);
    }
}

} // namespace unkxwyno::protocol
