# unkXwyno Mirror: Binary Packet Framing Specification

**Version**: `1.0.0`  
**Status**: Official Specification  
**Namespace**: `unkXwyno`  
**URI**: `https://unkxwyno-mirror.genzhost.best`

---

## 1. Overview
The **unkXwyno Mirror** protocol is an ultra-low-overhead binary transport format designed to stream high-framerate (60Hz, 120Hz, 144Hz, 240Hz) video, 48kHz audio, and sub-1ms multi-touch digitizer events over raw USB bulk endpoints and local network sockets.

It imposes a fixed **8-byte header** ahead of every payload frame, eliminating all parsing overhead, JSON deserialization latency, and variable-length delimiter scanning.

---

## 2. Fixed 8-Byte Packet Header Layout

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Body Data Length                        |
|                         (32 bits, LE)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Channel ID  |  Packet Type  |    Sequence   |    Reserved   |
|     (8 bits)  |    (8 bits)   |    (8 bits)   |  (0x00, 8 bits)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                   Payload (Body Data Length Bytes)            |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Field Definitions

| Offset | Field Name | Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00 - 0x03` | `Body Data Length` | `uint32_t` (Little-Endian) | Size in bytes of the payload immediately following the header. Maximum allowed value is `16,777,216` (16 MiB). |
| `0x04` | `Channel ID` | `uint8_t` | Identifies the logical multiplexing channel (`0x00` = Control, `0x01` = Video, `0x02` = Audio, `0x03` = Input). |
| `0x05` | `Packet Type` | `uint8_t` | Type indicator for the sub-channel (e.g. Keyframe, Slice, PCM, Touch). |
| `0x06` | `Sequence` | `uint8_t` | Monotonically increasing wrapping counter (`0x00` to `0xFF`) for loss detection and reordering validation. |
| `0x07` | `Reserved` | `uint8_t` | Alignment padding byte. Must be set to `0x00`. |

---

## 3. Channels & Packet Types

### 3.1 Control Channel (`Channel ID = 0x00`)
Used for session negotiation, latency probing, and dynamic encoder adaptation.

| Packet Type | Name | Payload Structure | Description |
| :--- | :--- | :--- | :--- |
| `0x80` | `UNKXWYNO_CTRL_PING` | `uint64_t send_time_us`, `uint64_t echo_time_us` | Heartbeat & RTT measurement probe. |
| `0x81` | `UNKXWYNO_CTRL_PONG` | `uint64_t send_time_us`, `uint64_t echo_time_us` | Heartbeat response with host echo timestamp. |
| `0x82` | `UNKXWYNO_CTRL_REQ_KEYFRAME` | *Empty* | Host decoder signals packet loss or corruption; requests sender to generate an immediate IDR keyframe. |
| `0x83` | `UNKXWYNO_CTRL_SET_BITRATE` | `uint32_t bitrate_bps` (LE) | Dynamically adjusts the mobile encoder target bitrate. |
| `0x84` | `UNKXWYNO_CTRL_SET_FPS` | `uint32_t target_fps` (LE) | Dynamically requests framerate changes (60, 120, 144, 240). |
| `0x85` | `UNKXWYNO_CTRL_ROTATION_SYNC` | `uint16_t orientation` (LE) | Reports display rotation angles (`0`, `90`, `180`, `270`). |
| `0x8F` | `UNKXWYNO_CTRL_DISCONNECT` | *Empty* | Signals graceful teardown. |

### 3.2 Video Channel (`Channel ID = 0x01`)
Transmits raw H.264/H.265 Annex B NAL units (`0x00000001` or `0x000001` prefixed).

| Packet Type | Name | Payload Content |
| :--- | :--- | :--- |
| `0x01` | `UNKXWYNO_VIDEO_CONFIG` | SPS / PPS / VPS parameter sets required to initialize hardware decoders. |
| `0x02` | `UNKXWYNO_VIDEO_KEYFRAME` | Full IDR / CRA frame slice payload. |
| `0x03` | `UNKXWYNO_VIDEO_DELTA` | Non-IDR P-Frame / B-Frame slice payload. |
| `0x04` | `UNKXWYNO_VIDEO_METADATA` | `unkxwyno_video_metadata_t` (Width, Height, FPS, Codec, Orientation, PTS). |

### 3.3 Audio Channel (`Channel ID = 0x02`)
Streams internal system/game audio recorded from `AudioPlaybackCapture` or AirPlay ALAC.

| Packet Type | Name | Payload Content |
| :--- | :--- | :--- |
| `0x10` | `UNKXWYNO_AUDIO_CONFIG` | `unkxwyno_audio_metadata_t` (Sample Rate, Channels, Codec). |
| `0x11` | `UNKXWYNO_AUDIO_PCM_S16LE` | Interleaved 16-bit 48kHz Stereo PCM audio samples. |
| `0x12` | `UNKXWYNO_AUDIO_AAC_RAW` | Raw AAC audio frame data. |
| `0x13` | `UNKXWYNO_AUDIO_OPUS` | Low-latency 2.5ms / 5ms Opus packet. |
| `0x14` | `UNKXWYNO_AUDIO_ALAC` | Apple Lossless Audio Codec frame (from iOS AirPlay). |

### 3.4 Input Channel (`Channel ID = 0x03`)
Injected from PC mouse and keyboard directly into Android (via AOAv2 HID Multi-touch digitizer) or iOS (via BLE HOGP).

| Packet Type | Name | Payload Structure |
| :--- | :--- | :--- |
| `0x20` | `UNKXWYNO_INPUT_TOUCH_EVENT` | `pointer_id` (1B), `action` (1B), `x` (2B LE), `y` (2B LE), `pressure` (2B LE), `timestamp_us` (8B LE). |
| `0x21` | `UNKXWYNO_INPUT_KEY_EVENT` | `key_code` (2B LE), `is_down` (1B), `modifiers` (1B). |
| `0x22` | `UNKXWYNO_INPUT_MOUSE_EVENT` | `delta_x` (2B LE), `delta_y` (2B LE), `wheel_v` (1B), `wheel_h` (1B), `buttons` (1B), `reserved` (1B). |
| `0x23` | `UNKXWYNO_INPUT_GAMEPAD_EVENT` | Gamepad axis sticks and button bitmask. |

---

## 4. Endianness & Conformance Rules
1. **Little-Endian Standard**: All 16-bit, 32-bit, and 64-bit numerical quantities in headers and control payloads MUST be serialized in Little-Endian byte order.
2. **Buffer Splitting**: If a video keyframe exceeds `UNKXWYNO_MAX_PACKET_PAYLOAD`, it MUST be segmented into consecutive packets with sequential sequence IDs.
3. **Zero Padding**: The reserved byte (`Offset 0x07`) MUST be transmitted as `0x00`.
