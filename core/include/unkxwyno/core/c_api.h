#pragma once

#include <stdint.h>

#if defined(_WIN32) && defined(UNKXWYNO_CORE_SHARED)
#  if defined(UNKXWYNO_CORE_BUILD)
#    define UXM_API __declspec(dllexport)
#  else
#    define UXM_API __declspec(dllimport)
#  endif
#else
#  define UXM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UXM_CORE_ABI_VERSION 1u
#define UXM_MAX_VIDEO_MODES 16u
#define UXM_MAX_CODECS 3u

typedef enum uxm_result {
    UXM_RESULT_OK = 0,
    UXM_RESULT_INVALID_ARGUMENT = 1,
    UXM_RESULT_INCOMPATIBLE_ABI = 2,
    UXM_RESULT_UNSUPPORTED = 3,
    UXM_RESULT_INVALID_TRANSITION = 4
} uxm_result;

typedef enum uxm_resolution_preset {
    UXM_RESOLUTION_AUTO = 0,
    UXM_RESOLUTION_480P = 1,
    UXM_RESOLUTION_HD = 2,
    UXM_RESOLUTION_FULL_HD = 3,
    UXM_RESOLUTION_QHD = 4,
    UXM_RESOLUTION_UHD = 5,
    UXM_RESOLUTION_NATIVE = 6,
    UXM_RESOLUTION_CUSTOM = 7
} uxm_resolution_preset;

typedef enum uxm_video_codec {
    UXM_CODEC_H264 = 1,
    UXM_CODEC_HEVC = 2,
    UXM_CODEC_AV1 = 3
} uxm_video_codec;

typedef enum uxm_session_state {
    UXM_SESSION_IDLE = 0,
    UXM_SESSION_DISCOVERING = 1,
    UXM_SESSION_PAIRING = 2,
    UXM_SESSION_CONNECTING = 3,
    UXM_SESSION_NEGOTIATING = 4,
    UXM_SESSION_STREAMING = 5,
    UXM_SESSION_RECOVERING = 6,
    UXM_SESSION_STOPPED = 7,
    UXM_SESSION_FAILED = 8
} uxm_session_state;

typedef enum uxm_session_event {
    UXM_EVENT_START_DISCOVERY = 0,
    UXM_EVENT_PEER_FOUND = 1,
    UXM_EVENT_PAIRING_ACCEPTED = 2,
    UXM_EVENT_TRANSPORT_CONNECTED = 3,
    UXM_EVENT_NEGOTIATION_ACCEPTED = 4,
    UXM_EVENT_TRANSPORT_LOST = 5,
    UXM_EVENT_TRANSPORT_RESTORED = 6,
    UXM_EVENT_STOP = 7,
    UXM_EVENT_FAIL = 8
} uxm_session_event;

typedef struct uxm_dimensions {
    uint32_t width;
    uint32_t height;
} uxm_dimensions;

typedef struct uxm_video_mode {
    uxm_dimensions dimensions;
    uint16_t frame_rates[7];
    uint32_t frame_rate_count;
} uxm_video_mode;

typedef struct uxm_codec_capability {
    uint32_t codec;
    uint8_t can_encode;
    uint8_t can_decode;
    uint8_t hardware_encode;
    uint8_t hardware_decode;
} uxm_codec_capability;

typedef struct uxm_endpoint_capabilities {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t protocol_major;
    uint32_t protocol_minor;
    uxm_dimensions native_dimensions;
    uxm_video_mode send_modes[UXM_MAX_VIDEO_MODES];
    uint32_t send_mode_count;
    uxm_video_mode receive_modes[UXM_MAX_VIDEO_MODES];
    uint32_t receive_mode_count;
    uxm_codec_capability codecs[UXM_MAX_CODECS];
    uint32_t codec_count;
    uint64_t max_send_bitrate_bps;
    uint64_t max_receive_bitrate_bps;
    uint16_t display_max_fps;
    uint8_t adaptive_bitrate;
} uxm_endpoint_capabilities;

typedef struct uxm_quality_request {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t resolution_preset;
    uxm_dimensions custom_dimensions;
    uint16_t frames_per_second;
    uint32_t codec_preference[UXM_MAX_CODECS];
    uint32_t codec_preference_count;
    uint8_t adaptive_bitrate;
    uint64_t requested_bitrate_bps;
    uint64_t maximum_bitrate_bps;
} uxm_quality_request;

typedef struct uxm_video_profile {
    uxm_dimensions dimensions;
    uint16_t frames_per_second;
    uint32_t codec;
    uint64_t bitrate_bps;
    uint8_t hardware_encode;
    uint8_t hardware_decode;
} uxm_video_profile;

typedef struct uxm_negotiation_result {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t status;
    uint32_t reason;
    uint8_t has_active_profile;
    uint8_t has_highest_alternative;
    uxm_video_profile active_profile;
    uxm_video_profile highest_alternative;
} uxm_negotiation_result;

typedef struct uxm_session uxm_session;

UXM_API uint32_t uxm_core_abi_version(void);
UXM_API const char* uxm_core_version_string(void);

UXM_API uxm_result uxm_negotiate_quality(
    const uxm_endpoint_capabilities* sender,
    const uxm_endpoint_capabilities* receiver,
    const uxm_quality_request* request,
    uxm_negotiation_result* result);

UXM_API uxm_session* uxm_session_create(void);
UXM_API void uxm_session_destroy(uxm_session* session);
UXM_API uxm_result uxm_session_transition(
    uxm_session* session,
    uint32_t event,
    uint64_t now_ms);
UXM_API uint32_t uxm_session_get_state(const uxm_session* session);
UXM_API void uxm_session_observe_heartbeat(uxm_session* session, uint64_t now_ms);
UXM_API void uxm_session_poll(uxm_session* session, uint64_t now_ms);
UXM_API uint64_t uxm_session_next_retry_delay_ms(const uxm_session* session);

#ifdef __cplusplus
}
#endif

