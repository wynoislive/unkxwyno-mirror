/**
 * @file unkxwyno_protocol.h
 * @brief Wire protocol and binary packet framing specification for unkXwyno Mirror.
 * @version 1.0.0
 * 
 * Target: Zero-dependency, ultra-low-latency screen, audio, and HID input mirroring
 * across Android (AOA2/ADB), iOS (usbmuxd/AirPlay), Windows, macOS, and Linux.
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#ifndef UNKXWYNO_PROTOCOL_H
#define UNKXWYNO_PROTOCOL_H

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * BRAND IDENTIFIERS & CONSTANTS
 * ============================================================================ */

#define UNKXWYNO_PROTOCOL_VERSION_MAJOR 1
#define UNKXWYNO_PROTOCOL_VERSION_MINOR 0
#define UNKXWYNO_PROTOCOL_VERSION_PATCH 0
#define UNKXWYNO_PROTOCOL_VERSION_HEX   0x010000

#define UNKXWYNO_BRAND_NAME             "unkXwyno Mirror"
#define UNKXWYNO_SUBDOMAIN_URI          "https://unkxwyno-mirror.genzhost.best"
#define UNKXWYNO_ANDROID_PACKAGE_ID     "best.genzhost.unkxwynomirror"

/* Android Open Accessory 2.0 (AOA2) Identification Strings */
#define UNKXWYNO_AOA_MANUFACTURER       "unkXwyno"
#define UNKXWYNO_AOA_MODEL              "unkXwyno Low-Latency Display"
#define UNKXWYNO_AOA_DESCRIPTION        "Ultra-Low-Latency Gaming Display"
#define UNKXWYNO_AOA_VERSION            "1.0.0"
#define UNKXWYNO_AOA_URI                "https://unkxwyno-mirror.genzhost.best"
#define UNKXWYNO_AOA_SERIAL             "UNKXWYNO-MIRROR-001"

/* iOS AirPlay & mDNS Identifiers */
#define UNKXWYNO_AIRPLAY_RECEIVER_NAME  "unkXwyno Mirror"
#define UNKXWYNO_AIRPLAY_MDNS_SERVICE   "_airplay._tcp"
#define UNKXWYNO_RAOP_MDNS_SERVICE      "_raop._tcp"
#define UNKXWYNO_AIRPLAY_DEFAULT_PORT   5000

/* USB Hardware Identifiers */
#define UNKXWYNO_USB_AOA_VID            0x18D1  /* Google Inc. */
#define UNKXWYNO_USB_AOA_PID_ACCESSORY  0x2D00  /* AOA Accessory Only */
#define UNKXWYNO_USB_AOA_PID_ACC_ADB    0x2D01  /* AOA Accessory + ADB */
#define UNKXWYNO_APPLE_USB_VID          0x05AC  /* Apple Inc. */

/* Protocol Limits & Constraints */
#define UNKXWYNO_PACKET_HEADER_SIZE     8
#define UNKXWYNO_MAX_PACKET_PAYLOAD     (16 * 1024 * 1024) /* 16 MiB max slice */
#define UNKXWYNO_DEFAULT_AUDIO_RATE     48000
#define UNKXWYNO_DEFAULT_AUDIO_CHANNELS 2
#define UNKXWYNO_MAX_TOUCH_POINTERS     10

/* ============================================================================
 * CHANNEL IDENTIFIERS
 * ============================================================================ */

typedef enum unkxwyno_channel_id {
    UNKXWYNO_CHANNEL_CONTROL  = 0x00, /* Handshake, stats, keyframe request, heartbeat */
    UNKXWYNO_CHANNEL_VIDEO    = 0x01, /* Video parameter sets, I/P/B-frames, slice NALUs */
    UNKXWYNO_CHANNEL_AUDIO    = 0x02, /* 48kHz internal audio frames (PCM, AAC, Opus, ALAC) */
    UNKXWYNO_CHANNEL_INPUT    = 0x03, /* Touchscreen digitizer, mouse, keyboard, gamepad */
    UNKXWYNO_CHANNEL_RESERVED = 0x04
} unkxwyno_channel_id_t;

/* ============================================================================
 * PACKET TYPES (BY CHANNEL)
 * ============================================================================ */

/* Control Packets (Channel 0x00) */
typedef enum unkxwyno_control_type {
    UNKXWYNO_CTRL_PING           = 0x80, /* Ping packet with sender timestamp */
    UNKXWYNO_CTRL_PONG           = 0x81, /* Pong response echoing timestamp */
    UNKXWYNO_CTRL_REQ_KEYFRAME   = 0x82, /* Decoder requesting immediate IDR keyframe */
    UNKXWYNO_CTRL_SET_BITRATE    = 0x83, /* Request encoder bitrate adjustment (bps) */
    UNKXWYNO_CTRL_SET_FPS        = 0x84, /* Request target framerate (60, 120, 144, 240) */
    UNKXWYNO_CTRL_ROTATION_SYNC  = 0x85, /* Device orientation sync (0, 90, 180, 270) */
    UNKXWYNO_CTRL_SET_SCREEN_POWER = 0x86, /* OLED pure-black screen-off mode (0=off, 1=on) */
    UNKXWYNO_CTRL_DISCONNECT     = 0x8F  /* Graceful session termination */
} unkxwyno_control_type_t;

/* Video Packets (Channel 0x01) */
typedef enum unkxwyno_video_type {
    UNKXWYNO_VIDEO_CONFIG        = 0x01, /* SPS / PPS / VPS decoder configuration record */
    UNKXWYNO_VIDEO_KEYFRAME      = 0x02, /* IDR / Keyframe slice */
    UNKXWYNO_VIDEO_DELTA         = 0x03, /* Non-IDR P-Frame / B-Frame slice */
    UNKXWYNO_VIDEO_METADATA      = 0x04  /* PTS/DTS timing, display width, height, codec */
} unkxwyno_video_type_t;

/* Video Codec Formats */
typedef enum unkxwyno_video_codec {
    UNKXWYNO_CODEC_H264          = 0x01, /* Advanced Video Coding (AVC) */
    UNKXWYNO_CODEC_H265          = 0x02, /* High Efficiency Video Coding (HEVC) */
    UNKXWYNO_CODEC_AV1           = 0x03  /* AOMedia Video 1 */
} unkxwyno_video_codec_t;

/* Audio Packets (Channel 0x02) */
typedef enum unkxwyno_audio_type {
    UNKXWYNO_AUDIO_CONFIG        = 0x10, /* Audio format descriptor (samplerate, channels) */
    UNKXWYNO_AUDIO_PCM_S16LE     = 0x11, /* Uncompressed 48kHz 16-bit Little-Endian PCM */
    UNKXWYNO_AUDIO_AAC_RAW       = 0x12, /* Raw AAC frames (AirPlay / MediaCodec) */
    UNKXWYNO_AUDIO_OPUS          = 0x13, /* Low-latency Opus packets (2.5ms / 5ms frame) */
    UNKXWYNO_AUDIO_ALAC          = 0x14  /* Apple Lossless Audio Codec (AirPlay wired) */
} unkxwyno_audio_type_t;

/* Input Packets (Channel 0x03) */
typedef enum unkxwyno_input_type {
    UNKXWYNO_INPUT_TOUCH_EVENT   = 0x20, /* Multi-touch digitizer point */
    UNKXWYNO_INPUT_KEY_EVENT     = 0x21, /* Keyboard scancode & modifier flags */
    UNKXWYNO_INPUT_MOUSE_EVENT   = 0x22, /* Relative motion, wheel, mouse buttons */
    UNKXWYNO_INPUT_GAMEPAD_EVENT = 0x23  /* Gamepad axis and button bitmask */
} unkxwyno_input_type_t;

/* Touch Action Flags */
typedef enum unkxwyno_touch_action {
    UNKXWYNO_TOUCH_DOWN          = 0x00,
    UNKXWYNO_TOUCH_MOVE          = 0x01,
    UNKXWYNO_TOUCH_UP            = 0x02,
    UNKXWYNO_TOUCH_CANCEL        = 0x03
} unkxwyno_touch_action_t;

/* ============================================================================
 * WIRE BINARY STRUCTURES (STRICT 1-BYTE ALIGNMENT)
 * ============================================================================ */

#if defined(_MSC_VER)
    #pragma pack(push, 1)
    #define UNKXWYNO_PACKED
#elif defined(__GNUC__) || defined(__clang__)
    #define UNKXWYNO_PACKED __attribute__((packed))
#else
    #define UNKXWYNO_PACKED
#endif

/**
 * @brief Fixed 8-byte framing header preceding every payload on the wire.
 * Layout:
 *   [0..3] uint32_t body_len    (Little-Endian byte size of payload)
 *   [4]    uint8_t  channel_id  (0=Control, 1=Video, 2=Audio, 3=Input)
 *   [5]    uint8_t  packet_type (Specific packet type per channel)
 *   [6]    uint8_t  sequence    (Monotonically increasing wrapping counter)
 *   [7]    uint8_t  reserved    (Alignment padding, must be 0x00)
 */
typedef struct UNKXWYNO_PACKED unkxwyno_packet_header {
    uint32_t body_len;
    uint8_t  channel_id;
    uint8_t  packet_type;
    uint8_t  sequence;
    uint8_t  reserved;
} unkxwyno_packet_header_t;

/**
 * @brief Video frame metadata header payload (transmitted with UNKXWYNO_VIDEO_METADATA).
 */
typedef struct UNKXWYNO_PACKED unkxwyno_video_metadata {
    uint32_t width;
    uint32_t height;
    uint32_t target_fps;
    uint8_t  codec;             /* unkxwyno_video_codec_t */
    uint8_t  orientation;       /* 0, 90, 180, 270 degrees */
    uint16_t reserved;
    uint64_t presentation_pts;  /* PTS in microseconds */
} unkxwyno_video_metadata_t;

/**
 * @brief Audio metadata header payload (transmitted with UNKXWYNO_AUDIO_CONFIG).
 */
typedef struct UNKXWYNO_PACKED unkxwyno_audio_metadata {
    uint32_t sample_rate;       /* e.g. 48000 */
    uint8_t  channels;          /* 1 = Mono, 2 = Stereo */
    uint8_t  bits_per_sample;   /* 16 or 24 */
    uint8_t  audio_codec;       /* unkxwyno_audio_type_t */
    uint8_t  reserved;
} unkxwyno_audio_metadata_t;

/**
 * @brief Multi-touch digitizer point payload (transmitted with UNKXWYNO_INPUT_TOUCH_EVENT).
 */
typedef struct UNKXWYNO_PACKED unkxwyno_touch_event {
    uint8_t  pointer_id;        /* Finger/contact tracking index (0..9) */
    uint8_t  action;            /* unkxwyno_touch_action_t */
    uint16_t x;                 /* Normalized coordinate 0..65535 */
    uint16_t y;                 /* Normalized coordinate 0..65535 */
    uint16_t pressure;          /* Normalized pressure 0..1024 */
    uint64_t timestamp_us;      /* Event host timestamp */
} unkxwyno_touch_event_t;

/**
 * @brief Keyboard key event payload (transmitted with UNKXWYNO_INPUT_KEY_EVENT).
 */
typedef struct UNKXWYNO_PACKED unkxwyno_key_event {
    uint16_t key_code;          /* USB HID usage code / scan code */
    uint8_t  is_down;           /* 1 = Pressed, 0 = Released */
    uint8_t  modifiers;         /* Shift=1, Ctrl=2, Alt=4, Meta=8 */
} unkxwyno_key_event_t;

/**
 * @brief Mouse motion/click payload (transmitted with UNKXWYNO_INPUT_MOUSE_EVENT).
 */
typedef struct UNKXWYNO_PACKED unkxwyno_mouse_event {
    int16_t  delta_x;           /* Relative X motion */
    int16_t  delta_y;           /* Relative Y motion */
    int8_t   wheel_v;           /* Vertical scroll step */
    int8_t   wheel_h;           /* Horizontal scroll step */
    uint8_t  buttons;           /* Bitmask: Left=1, Right=2, Middle=4 */
    uint8_t  reserved;
} unkxwyno_mouse_event_t;

/**
 * @brief Control message ping/pong payload.
 */
typedef struct UNKXWYNO_PACKED unkxwyno_control_ping {
    uint64_t send_timestamp_us;
    uint64_t echo_timestamp_us;
} unkxwyno_control_ping_t;

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

/* ============================================================================
 * INLINE SERIALIZATION & VALIDATION HELPERS
 * ============================================================================ */

/**
 * @brief Swap 32-bit integer for little-endian host conversion if needed.
 */
static inline uint32_t unkxwyno_to_le32(uint32_t val) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return ((val & 0xFF000000) >> 24) |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x000000FF) << 24);
#else
    return val;
#endif
}

/**
 * @brief Initialize a packet header with safe little-endian encoding.
 */
static inline void unkxwyno_header_init(unkxwyno_packet_header_t *hdr,
                                        uint8_t channel_id,
                                        uint8_t packet_type,
                                        uint32_t body_len,
                                        uint8_t sequence) {
    if (!hdr) return;
    hdr->body_len    = unkxwyno_to_le32(body_len);
    hdr->channel_id  = channel_id;
    hdr->packet_type = packet_type;
    hdr->sequence    = sequence;
    hdr->reserved    = 0x00;
}

/**
 * @brief Validate packet header bounds and constraints.
 */
static inline bool unkxwyno_header_is_valid(const unkxwyno_packet_header_t *hdr) {
    if (!hdr) return false;
    uint32_t len = unkxwyno_to_le32(hdr->body_len);
    if (len > UNKXWYNO_MAX_PACKET_PAYLOAD) return false;
    if (hdr->channel_id > UNKXWYNO_CHANNEL_INPUT && hdr->channel_id != UNKXWYNO_CHANNEL_RESERVED) {
        return false;
    }
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UNKXWYNO_PROTOCOL_H */
