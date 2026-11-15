/**
 * @file native_viewport.hpp
 * @brief Standalone native borderless display viewport for unkXwyno Mirror.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <string>
#include <functional>
#include <memory>
#include "../../../unkxwyno-protocol/include/unkxwyno_protocol.h"

namespace unkxwyno::renderer {

using TouchInputCallback = std::function<void(uint8_t pointer_id, uint8_t action, uint16_t x, uint16_t y, uint16_t pressure)>;
using KeyInputCallback   = std::function<void(uint16_t key_code, bool is_down, uint8_t modifiers)>;
using ResizeCallback     = std::function<void(int width, int height)>;

class INativeViewport {
public:
    virtual ~INativeViewport() = default;

    virtual bool Create(const std::string& title, int width, int height) = 0;
    virtual void Show(bool visible) = 0;
    virtual void SetFullscreen(bool fullscreen) = 0;
    virtual void Close() = 0;

    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
    [[nodiscard]] virtual bool IsOpen() const = 0;

    virtual void SetTouchCallback(TouchInputCallback callback) = 0;
    virtual void SetKeyCallback(KeyInputCallback callback) = 0;
    virtual void SetResizeCallback(ResizeCallback callback) = 0;

    virtual void PollEvents() = 0;
};

std::unique_ptr<INativeViewport> CreatePlatformViewport();

} // namespace unkxwyno::renderer
