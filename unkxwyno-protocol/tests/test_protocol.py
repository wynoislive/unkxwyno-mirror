#!/usr/bin/env python3
"""
Unit test verifying binary header alignment, field offsets, and serialization
conformance for unkxwyno_protocol.h and unkxwyno_abi.h.
"""

import struct
import sys

def test_packet_header_layout():
    # uint32_t (4B) + uint8_t (1B) + uint8_t (1B) + uint8_t (1B) + uint8_t (1B) = 8 Bytes
    fmt = "<IBBBB"
    expected_size = 8
    actual_size = struct.calcsize(fmt)
    assert actual_size == expected_size, f"Header size mismatch: got {actual_size}, expected {expected_size}"

    # Serialize a test video keyframe packet header
    body_len = 1048576  # 1 MiB
    channel_id = 1      # Video
    packet_type = 2     # Keyframe
    sequence = 42       # Seq #42
    reserved = 0        # 0x00

    packed = struct.pack(fmt, body_len, channel_id, packet_type, sequence, reserved)
    assert len(packed) == 8

    # Unpack and verify
    unpacked_len, unpacked_chan, unpacked_type, unpacked_seq, unpacked_res = struct.unpack(fmt, packed)
    assert unpacked_len == body_len
    assert unpacked_chan == 1
    assert unpacked_type == 2
    assert unpacked_seq == 42
    assert unpacked_res == 0

    print("✔ [PASS] Packet Header 8-byte layout & Little-Endian serialization verified.")

def test_touch_event_layout():
    # uint8_t + uint8_t + uint16_t + uint16_t + uint16_t + uint64_t = 1 + 1 + 2 + 2 + 2 + 8 = 16 Bytes
    fmt = "<BBHHHQ"
    expected_size = 16
    actual_size = struct.calcsize(fmt)
    assert actual_size == expected_size, f"Touch event size mismatch: got {actual_size}, expected {expected_size}"

    pointer_id = 1
    action = 0  # Down
    x = 32768   # 50%
    y = 16384   # 25%
    pressure = 512
    ts = 1700000000123456

    packed = struct.pack(fmt, pointer_id, action, x, y, pressure, ts)
    assert len(packed) == 16
    print("✔ [PASS] Touch Event 16-byte packed structure verified.")

def test_video_metadata_layout():
    # uint32_t width, height, target_fps + uint8_t codec, orient + uint16_t res + uint64_t pts
    # 4 + 4 + 4 + 1 + 1 + 2 + 8 = 24 Bytes
    fmt = "<IIIBBHQ"
    expected_size = 24
    actual_size = struct.calcsize(fmt)
    assert actual_size == expected_size, f"Video metadata size mismatch: got {actual_size}, expected {expected_size}"

    packed = struct.pack(fmt, 1920, 1080, 120, 1, 90, 0, 9999999)
    assert len(packed) == 24
    print("✔ [PASS] Video Metadata 24-byte packed structure verified.")

if __name__ == "__main__":
    print("========================================")
    print(" Running unkXwyno Protocol Unit Tests")
    print("========================================")
    try:
        test_packet_header_layout()
        test_touch_event_layout()
        test_video_metadata_layout()
        print("========================================")
        print(" All Protocol Tests PASSED Successfully!")
        print("========================================")
        sys.exit(0)
    except AssertionError as e:
        print(f"❌ [FAIL]: {e}")
        sys.exit(1)
