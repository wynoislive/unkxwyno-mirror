/**
 * @file video_renderer.hpp
 * @brief Abstract interface for OS-native GPU swapchain video renderers.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include <cstdint>
#include <memory>
#include "../decoder/video_decoder.hpp"

namespace unkxwyno::renderer {

class IVideoRenderer {
public:
    virtual ~IVideoRenderer() = default;

    virtual bool Initialize(void* native_window_handle, int width, int height) = 0;
    virtual void Shutdown() = 0;

    /**
     * @brief Render a hardware or software decoded video frame onto the swapchain.
     */
    virtual void RenderFrame(const decoder::DecodedFrame& frame) = 0;

    virtual void Resize(int width, int height) = 0;
    virtual void SetFullscreen(bool fullscreen) = 0;

    [[nodiscard]] virtual void* GetNativeDevice() = 0;
};

} // namespace unkxwyno::renderer
