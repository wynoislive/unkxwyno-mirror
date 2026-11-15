/**
 * @file d3d11_renderer.hpp
 * @brief Direct3D 11.1 Flip-Discard Swapchain Hardware Renderer for Windows with In-Shader HUD.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#pragma once

#include "video_renderer.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>
#include <mutex>
#include <vector>

namespace unkxwyno::renderer {

class D3D11Renderer final : public IVideoRenderer {
public:
    D3D11Renderer();
    ~D3D11Renderer() override;

    bool Initialize(void* native_window_handle, int width, int height) override;
    void Shutdown() override;

    void RenderFrame(const decoder::DecodedFrame& frame) override;
    void Resize(int width, int height) override;
    void SetFullscreen(bool fullscreen) override;

    void SetKeymapBadges(const KeymapBadge* badges, size_t count) override;
    void SetHudVisible(bool visible) override;

    [[nodiscard]] void* GetNativeDevice() override {
        return d3d_device_.Get();
    }

    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const {
        return d3d_device_;
    }

private:
    bool CreateDeviceAndSwapChain(HWND hwnd, int width, int height);
    void CreateRenderTarget();

    HWND hwnd_{nullptr};
    int width_{1920};
    int height_{1080};
    bool is_initialized_{false};
    bool hud_visible_{true};

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;

    std::vector<KeymapBadge> badges_;
    std::mutex render_mutex_;
};

} // namespace unkxwyno::renderer
#endif // _WIN32
