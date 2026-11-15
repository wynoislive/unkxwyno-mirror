/**
 * @file unkxwyno_abi.h
 * @brief Public C ABI exports for unkXwyno Mirror Core Engine.
 * @version 1.0.0
 * 
 * Provides stable C ABI linkage for C# .NET 9 (Avalonia UI), Rust, and native clients.
 * Strict standard C99 ABI with no C++ name mangling.
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#ifndef UNKXWYNO_ABI_H
#define UNKXWYNO_ABI_H

#pragma once

#include "unkxwyno_protocol.h"

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(UNKXWYNO_EXPORTS)
        #define UNKXWYNO_API __declspec(dllexport)
    #else
        #define UNKXWYNO_API __declspec(dllimport)
    #endif
    #define UNKXWYNO_CALL __cdecl
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define UNKXWYNO_API __attribute__((visibility("default")))
    #else
        #define UNKXWYNO_API
    #endif
    #define UNKXWYNO_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RETURN CODES & STATUS
 * ============================================================================ */

typedef enum unkxwyno_status {
    UNKXWYNO_SUCCESS               =  0,
    UNKXWYNO_ERR_INVALID_PARAM     = -1,
    UNKXWYNO_ERR_OUT_OF_MEMORY     = -2,
    UNKXWYNO_ERR_DEVICE_NOT_FOUND  = -3,
    UNKXWYNO_ERR_CONNECTION_FAILED = -4,
    UNKXWYNO_ERR_DECODER_INIT      = -5,
    UNKXWYNO_ERR_RENDERER_INIT     = -6,
    UNKXWYNO_ERR_AUDIO_INIT        = -7,
    UNKXWYNO_ERR_ALREADY_RUNNING   = -8,
    UNKXWYNO_ERR_NOT_RUNNING       = -9,
    UNKXWYNO_ERR_UNSUPPORTED       = -10
} unkxwyno_status_t;

/* Connection Modes */
typedef enum unkxwyno_connection_mode {
    UNKXWYNO_CONN_AUTO             = 0, /* Auto-detect attached USB / network device */
    UNKXWYNO_CONN_ANDROID_AOA2     = 1, /* Android Open Accessory 2.0 (Zero USB debugging) */
    UNKXWYNO_CONN_ANDROID_ADB      = 2, /* Native C++ ADB client (High-refresh 120Hz/240Hz) */
    UNKXWYNO_CONN_IOS_USB          = 3, /* iOS over USB via native usbmuxd tunnel */
    UNKXWYNO_CONN_IOS_WIFI         = 4  /* iOS over Wi-Fi via mDNS AirPlay */
} unkxwyno_connection_mode_t;

/* Device Connection States */
typedef enum unkxwyno_device_state {
    UNKXWYNO_STATE_DISCONNECTED    = 0,
    UNKXWYNO_STATE_CONNECTING      = 1,
    UNKXWYNO_STATE_CONNECTED       = 2,
    UNKXWYNO_STATE_STREAMING       = 3,
    UNKXWYNO_STATE_ERROR           = 4
} unkxwyno_device_state_t;

/* Log Levels */
typedef enum unkxwyno_log_level {
    UNKXWYNO_LOG_DEBUG             = 0,
    UNKXWYNO_LOG_INFO              = 1,
    UNKXWYNO_LOG_WARN              = 2,
    UNKXWYNO_LOG_ERROR             = 3
} unkxwyno_log_level_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/* Opaque Engine Instance Handle */
typedef struct unkxwyno_engine* unkxwyno_engine_t;

/* Device Information Record */
typedef struct unkxwyno_device_info {
    char     device_id[64];       /* Unique identifier (USB serial or address) */
    char     model_name[64];      /* Friendly device model */
    char     manufacturer[64];    /* Manufacturer string */
    uint8_t  device_type;         /* 1 = Android, 2 = iOS */
    uint8_t  connection_state;    /* unkxwyno_device_state_t */
    uint16_t reserved;
} unkxwyno_device_info_t;

/* Real-Time Performance Telemetry */
typedef struct unkxwyno_metrics {
    float    current_fps;            /* Instantaneous rendered framerate */
    uint32_t bitrate_bps;            /* Current video payload bandwidth */
    float    decode_latency_ms;      /* Glass-to-glass hardware decode latency */
    float    render_latency_ms;      /* Swapchain present latency */
    float    audio_buffer_ms;        /* Current audio jitter queue depth */
    uint64_t total_frames_rendered;
    uint64_t total_frames_dropped;
} unkxwyno_metrics_t;

/* Configuration Parameters */
typedef struct unkxwyno_config {
    uint32_t max_width;              /* Max display width (e.g. 1920 or 2560) */
    uint32_t max_height;             /* Max display height (e.g. 1080 or 1440) */
    uint32_t target_fps;             /* Target framerate (60, 120, 144, 240) */
    uint32_t bitrate_bps;            /* Max target bitrate (e.g. 20000000 = 20 Mbps) */
    uint8_t  connection_mode;        /* unkxwyno_connection_mode_t */
    uint8_t  enable_audio;           /* 1 = Enabled (WASAPI/CoreAudio), 0 = Muted */
    uint8_t  enable_touch_input;     /* 1 = Enabled (AOAv2 HID Multi-touch), 0 = Disabled */
    uint8_t  borderless_viewport;    /* 1 = Native Borderless Window, 0 = Embedded */
} unkxwyno_config_t;

/* ============================================================================
 * INTEROP CALLBACK SIGNATURES
 * ============================================================================ */

/* State change notification */
typedef void (UNKXWYNO_CALL *unkxwyno_state_cb)(
    const unkxwyno_device_info_t* device,
    unkxwyno_device_state_t state,
    void* user_data
);

/* Performance telemetry update */
typedef void (UNKXWYNO_CALL *unkxwyno_metrics_cb)(
    const unkxwyno_metrics_t* metrics,
    void* user_data
);

/* Engine log message */
typedef void (UNKXWYNO_CALL *unkxwyno_log_cb)(
    unkxwyno_log_level_t level,
    const char* message,
    void* user_data
);

/* Video frame pixel notification (optional, for thumbnail / software capture) */
typedef void (UNKXWYNO_CALL *unkxwyno_frame_cb)(
    const uint8_t* pixel_data,
    int width,
    int height,
    int stride,
    uint64_t pts_us,
    void* user_data
);

/* ============================================================================
 * ENGINE LIFECYCLE & CORE EXPORTS
 * ============================================================================ */

/**
 * @brief Get the semantic version string of the engine (e.g. "1.0.0").
 */
UNKXWYNO_API const char* UNKXWYNO_CALL unkxwyno_get_version(void);

/**
 * @brief Create a new instance of the unkXwyno Core Engine.
 * @param config Initial engine configuration.
 * @param out_engine Output pointer to receive the opaque handle.
 * @return UNKXWYNO_SUCCESS on success.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_engine_create(
    const unkxwyno_config_t* config,
    unkxwyno_engine_t* out_engine
);

/**
 * @brief Destroy the engine instance and release all hardware/USB resources.
 */
UNKXWYNO_API void UNKXWYNO_CALL unkxwyno_engine_destroy(
    unkxwyno_engine_t engine
);

/**
 * @brief Register listener callbacks for the engine.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_set_callbacks(
    unkxwyno_engine_t engine,
    unkxwyno_state_cb state_callback,
    unkxwyno_metrics_cb metrics_callback,
    unkxwyno_log_cb log_callback,
    void* user_data
);

/**
 * @brief Start the device scanning and mirroring service.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_start(
    unkxwyno_engine_t engine
);

/**
 * @brief Stop active mirroring and disconnect active devices.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_stop(
    unkxwyno_engine_t engine
);

/**
 * @brief Retrieve current instantaneous telemetry metrics.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_get_metrics(
    unkxwyno_engine_t engine,
    unkxwyno_metrics_t* out_metrics
);

/* ============================================================================
 * NATIVE VIEWPORT & RENDERING EXPORTS
 * ============================================================================ */

/**
 * @brief Create and show the standalone borderless native display window.
 * @param engine Active engine handle.
 * @param title Window caption string (e.g. "unkXwyno Mirror - Low Latency Display").
 * @param initial_width Initial window width in pixels.
 * @param initial_height Initial window height in pixels.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_viewport_create(
    unkxwyno_engine_t engine,
    const char* title,
    int initial_width,
    int initial_height
);

/**
 * @brief Set viewport visibility.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_viewport_show(
    unkxwyno_engine_t engine,
    bool visible
);

/**
 * @brief Toggle true fullscreen mode on the active monitor.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_viewport_set_fullscreen(
    unkxwyno_engine_t engine,
    bool fullscreen
);

/**
 * @brief Get the native OS window handle (HWND on Windows, NSWindow* on macOS, Window on X11).
 */
UNKXWYNO_API void* UNKXWYNO_CALL unkxwyno_viewport_get_native_handle(
    unkxwyno_engine_t engine
);

/* ============================================================================
 * INPUT INJECTION EXPORTS (SUB-1MS HARDWARE DISPATCH)
 * ============================================================================ */

/**
 * @brief Inject a multi-touch digitizer event to the mobile device.
 * @param engine Active engine handle.
 * @param pointer_id Contact index (0..9).
 * @param action 0 = Down, 1 = Move, 2 = Up, 3 = Cancel.
 * @param x Normalized coordinate across display width (0..65535).
 * @param y Normalized coordinate across display height (0..65535).
 * @param pressure Normalized touch pressure (0..1024).
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_inject_touch(
    unkxwyno_engine_t engine,
    uint8_t pointer_id,
    uint8_t action,
    uint16_t x,
    uint16_t y,
    uint16_t pressure
);

/**
 * @brief Inject a keyboard keystroke to the mobile device.
 * @param engine Active engine handle.
 * @param key_code USB HID standard key code or Android scancode.
 * @param is_down 1 = Key down, 0 = Key up.
 * @param modifiers Modifier bitmask (Shift, Ctrl, Alt).
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_inject_key(
    unkxwyno_engine_t engine,
    uint16_t key_code,
    bool is_down,
    uint8_t modifiers
);

/**
 * @brief Inject relative mouse motion or mouse button click.
 */
UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_inject_mouse(
    unkxwyno_engine_t engine,
    int16_t delta_x,
    int16_t delta_y,
    int8_t wheel_v,
    int8_t wheel_h,
    uint8_t buttons
);

#ifdef __cplusplus
}
#endif

#endif /* UNKXWYNO_ABI_H */
