/**
 * @file test_demuxer.cpp
 * @brief Unit tests for unkXwyno ProtocolDemuxer and chunk fragmentation handling.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include "../src/protocol/demuxer.hpp"

int main() {
    std::printf("========================================\n");
    std::printf(" unkXwyno Core: Protocol Demuxer Tests\n");
    std::printf("========================================\n");

    unkxwyno::protocol::ProtocolDemuxer demuxer;

    std::vector<std::string> received_video_frames;
    std::vector<std::string> received_audio_frames;
    std::vector<uint8_t> received_ctrl_types;

    demuxer.SetVideoHandler([&](uint8_t type, std::span<const uint8_t> payload, uint8_t seq) {
        received_video_frames.emplace_back(reinterpret_cast<const char*>(payload.data()), payload.size());
    });

    demuxer.SetAudioHandler([&](uint8_t type, std::span<const uint8_t> payload, uint8_t seq) {
        received_audio_frames.emplace_back(reinterpret_cast<const char*>(payload.data()), payload.size());
    });

    demuxer.SetControlHandler([&](uint8_t type, std::span<const uint8_t> payload) {
        received_ctrl_types.push_back(type);
    });

    // Test 1: Complete Video Packet
    std::string video_data = "NALU_IDR_KEYFRAME_1234567890";
    unkxwyno_packet_header_t hdr1;
    unkxwyno_header_init(&hdr1, UNKXWYNO_CHANNEL_VIDEO, UNKXWYNO_VIDEO_KEYFRAME, static_cast<uint32_t>(video_data.size()), 1);

    std::vector<uint8_t> pkt1(sizeof(hdr1) + video_data.size());
    std::memcpy(pkt1.data(), &hdr1, sizeof(hdr1));
    std::memcpy(pkt1.data() + sizeof(hdr1), video_data.data(), video_data.size());

    demuxer.PushData(pkt1);

    assert(received_video_frames.size() == 1);
    assert(received_video_frames[0] == video_data);
    std::printf("✔ [PASS] Single complete video frame demuxed successfully.\n");

    // Test 2: Fragmented Audio Packet Split Across 3 Tiny USB Chunks
    std::string audio_data = "PCM_48KHZ_STEREO_SAMPLES_ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unkxwyno_packet_header_t hdr2;
    unkxwyno_header_init(&hdr2, UNKXWYNO_CHANNEL_AUDIO, UNKXWYNO_AUDIO_PCM_S16LE, static_cast<uint32_t>(audio_data.size()), 2);

    std::vector<uint8_t> pkt2(sizeof(hdr2) + audio_data.size());
    std::memcpy(pkt2.data(), &hdr2, sizeof(hdr2));
    std::memcpy(pkt2.data() + sizeof(hdr2), audio_data.data(), audio_data.size());

    // Split pkt2 into chunk A (first 4 bytes of header), chunk B (next 10 bytes), chunk C (remainder)
    std::vector<uint8_t> chunkA(pkt2.begin(), pkt2.begin() + 4);
    std::vector<uint8_t> chunkB(pkt2.begin() + 4, pkt2.begin() + 14);
    std::vector<uint8_t> chunkC(pkt2.begin() + 14, pkt2.end());

    demuxer.PushData(chunkA);
    assert(received_audio_frames.empty()); // Incomplete header, no dispatch

    demuxer.PushData(chunkB);
    assert(received_audio_frames.empty()); // Partial body, no dispatch

    demuxer.PushData(chunkC);
    assert(received_audio_frames.size() == 1);
    assert(received_audio_frames[0] == audio_data);
    std::printf("✔ [PASS] 3-way chunk fragmented audio packet reassembled and demuxed.\n");

    // Test 3: Control Packet
    unkxwyno_packet_header_t hdr3;
    unkxwyno_header_init(&hdr3, UNKXWYNO_CHANNEL_CONTROL, UNKXWYNO_CTRL_PING, 0, 3);
    std::vector<uint8_t> pkt3(sizeof(hdr3));
    std::memcpy(pkt3.data(), &hdr3, sizeof(hdr3));

    demuxer.PushData(pkt3);
    assert(received_ctrl_types.size() == 1);
    assert(received_ctrl_types[0] == UNKXWYNO_CTRL_PING);
    std::printf("✔ [PASS] Control PING packet processed successfully.\n");

    assert(demuxer.GetTotalPacketsParsed() == 3);
    assert(demuxer.GetDroppedPackets() == 0);

    std::printf("========================================\n");
    std::printf(" All Demuxer Tests PASSED!\n");
    std::printf("========================================\n");
    return 0;
}
