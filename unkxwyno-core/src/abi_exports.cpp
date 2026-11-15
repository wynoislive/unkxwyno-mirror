/**
 * @file abi_exports.cpp
 * @brief Public C ABI exports implementation wrapping unkXwyno Core Engine.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#ifndef UNKXWYNO_EXPORTS
#define UNKXWYNO_EXPORTS
#endif
#include "../../unkxwyno-protocol/include/unkxwyno_abi.h"
#include "../include/unkxwyno_core.hpp"

// Definition of opaque struct wrapper
struct unkxwyno_engine {
    std::unique_ptr<unkxwyno::Engine> impl;
};

extern "C" {

UNKXWYNO_API const char* UNKXWYNO_CALL unkxwyno_get_version(void) {
    return "1.0.0";
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_engine_create(
    const unkxwyno_config_t* config,
    unkxwyno_engine_t* out_engine
) {
    if (!config || !out_engine) {
        return UNKXWYNO_ERR_INVALID_PARAM;
    }

    try {
        auto engine = new unkxwyno_engine();
        engine->impl = std::make_unique<unkxwyno::Engine>(*config);
        *out_engine = engine;
        return UNKXWYNO_SUCCESS;
    } catch (...) {
        return UNKXWYNO_ERR_OUT_OF_MEMORY;
    }
}

UNKXWYNO_API void UNKXWYNO_CALL unkxwyno_engine_destroy(
    unkxwyno_engine_t engine
) {
    if (engine) {
        delete engine;
    }
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_set_callbacks(
    unkxwyno_engine_t engine,
    unkxwyno_state_cb state_callback,
    unkxwyno_metrics_cb metrics_callback,
    unkxwyno_log_cb log_callback,
    void* user_data
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->SetCallbacks(state_callback, metrics_callback, log_callback, user_data);
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_start(
    unkxwyno_engine_t engine
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->Start();
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_stop(
    unkxwyno_engine_t engine
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->Stop();
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_get_metrics(
    unkxwyno_engine_t engine,
    unkxwyno_metrics_t* out_metrics
) {
    if (!engine || !engine->impl || !out_metrics) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->GetMetrics(out_metrics);
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_viewport_create(
    unkxwyno_engine_t engine,
    const char* title,
    int initial_width,
    int initial_height
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->ViewportCreate(title, initial_width, initial_height);
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_viewport_show(
    unkxwyno_engine_t engine,
    bool visible
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->ViewportShow(visible);
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_viewport_set_fullscreen(
    unkxwyno_engine_t engine,
    bool fullscreen
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->ViewportSetFullscreen(fullscreen);
}

UNKXWYNO_API void* UNKXWYNO_CALL unkxwyno_viewport_get_native_handle(
    unkxwyno_engine_t engine
) {
    if (!engine || !engine->impl) return nullptr;
    return engine->impl->ViewportGetNativeHandle();
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_inject_touch(
    unkxwyno_engine_t engine,
    uint8_t pointer_id,
    uint8_t action,
    uint16_t x,
    uint16_t y,
    uint16_t pressure
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->InjectTouch(pointer_id, action, x, y, pressure);
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_inject_key(
    unkxwyno_engine_t engine,
    uint16_t key_code,
    bool is_down,
    uint8_t modifiers
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->InjectKey(key_code, is_down, modifiers);
}

UNKXWYNO_API unkxwyno_status_t UNKXWYNO_CALL unkxwyno_inject_mouse(
    unkxwyno_engine_t engine,
    int16_t delta_x,
    int16_t delta_y,
    int8_t wheel_v,
    int8_t wheel_h,
    uint8_t buttons
) {
    if (!engine || !engine->impl) return UNKXWYNO_ERR_INVALID_PARAM;
    return engine->impl->InjectMouse(delta_x, delta_y, wheel_v, wheel_h, buttons);
}

} // extern "C"
