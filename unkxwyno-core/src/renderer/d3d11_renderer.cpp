/**
 * @file d3d11_renderer.cpp
 * @brief Implementation of D3D11Renderer with unthrottled FLIP_DISCARD swapchain.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "d3d11_renderer.hpp"

#ifdef _WIN32
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace unkxwyno::renderer {

using Microsoft::WRL::ComPtr;

D3D11Renderer::D3D11Renderer() = default;

D3D11Renderer::~D3D11Renderer() {
    Shutdown();
}

bool D3D11Renderer::Initialize(void* native_window_handle, int width, int height) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    if (is_initialized_) return true;

    hwnd_   = static_cast<HWND>(native_window_handle);
    width_  = width > 0 ? width : 1920;
    height_ = height > 0 ? height : 1080;

    if (!CreateDeviceAndSwapChain(hwnd_, width_, height_)) {
        return false;
    }

    CreateRenderTarget();
    is_initialized_ = true;
    is_running_ = true;
    has_received_frames_ = false;

    // Immediately present an initial sleek obsidian gaming frame to prevent blank white viewport
    if (render_target_view_ && d3d_context_) {
        const float obsidian_clear[4] = { 0.035f, 0.043f, 0.059f, 1.0f }; // Sleek obsidian #090B0F
        d3d_context_->ClearRenderTargetView(render_target_view_.Get(), obsidian_clear);
        swap_chain_->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }

    // Launch active 30-60 FPS idle render loop until first video frame arrives
    idle_render_thread_ = std::thread(&D3D11Renderer::IdleRenderLoop, this);

    std::cout << "[D3D11 Renderer] Initialized FLIP_DISCARD swapchain with active idle HUD loop for " << width_ << "x" << height_ << "\n";
    return true;
}

void D3D11Renderer::PresentIdleFrame() {
    std::lock_guard<std::mutex> lock(render_mutex_);
    if (!is_initialized_ || !d3d_context_ || !swap_chain_ || !render_target_view_) return;
    const float obsidian_clear[4] = { 0.035f, 0.043f, 0.059f, 1.0f };
    d3d_context_->ClearRenderTargetView(render_target_view_.Get(), obsidian_clear);
    swap_chain_->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

void D3D11Renderer::IdleRenderLoop() {
    while (is_running_ && !has_received_frames_) {
        PresentIdleFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS low-CPU idle heartbeat
    }
}

void D3D11Renderer::Shutdown() {
    is_running_ = false;
    has_received_frames_ = true;
    if (idle_render_thread_.joinable()) {
        idle_render_thread_.join();
    }

    std::lock_guard<std::mutex> lock(render_mutex_);
    if (!is_initialized_) return;

    render_target_view_.Reset();
    software_texture_.Reset();
    software_tex_w_ = 0;
    software_tex_h_ = 0;
    swap_chain_.Reset();
    d3d_context_.Reset();
    d3d_device_.Reset();
    is_initialized_ = false;
}

bool D3D11Renderer::CreateDeviceAndSwapChain(HWND hwnd, int width, int height) {
    UINT create_flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if defined(_DEBUG)
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL feature_level;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &d3d_device_,
        &feature_level,
        &d3d_context_
    );

    if (FAILED(hr)) {
        // Fallback without video flag or debug flag
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            &d3d_device_,
            &feature_level,
            &d3d_context_
        );
    }

    if (FAILED(hr) || !d3d_device_) {
        std::cerr << "[D3D11] D3D11CreateDevice failed (hr=" << std::hex << hr << ")\n";
        return false;
    }

    if (!hwnd) {
        // Headless or window will be attached later
        return true;
    }

    // Query DXGI Factory to create modern flip-model swap chain
    ComPtr<IDXGIDevice> dxgi_device;
    d3d_device_.As(&dxgi_device);

    ComPtr<IDXGIAdapter> adapter;
    dxgi_device->GetAdapter(&adapter);

    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 sc_desc{};
    sc_desc.Width              = static_cast<UINT>(width);
    sc_desc.Height             = static_cast<UINT>(height);
    sc_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc_desc.Stereo             = FALSE;
    sc_desc.SampleDesc.Count   = 1;
    sc_desc.SampleDesc.Quality = 0;
    sc_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount        = 2; // Double buffering for minimum input lag
    sc_desc.Scaling            = DXGI_SCALING_NONE;
    sc_desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Modern low-latency flip model
    sc_desc.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
    sc_desc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Uncapped framerate / G-Sync / FreeSync

    hr = factory->CreateSwapChainForHwnd(
        d3d_device_.Get(),
        hwnd,
        &sc_desc,
        nullptr,
        nullptr,
        &swap_chain_
    );

    if (FAILED(hr)) {
        // Fallback to bitblt swap effect if flip discard not supported
        sc_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sc_desc.Scaling    = DXGI_SCALING_STRETCH;
        sc_desc.Flags      = 0;
        hr = factory->CreateSwapChainForHwnd(d3d_device_.Get(), hwnd, &sc_desc, nullptr, nullptr, &swap_chain_);
    }

    return SUCCEEDED(hr) && swap_chain_;
}

void D3D11Renderer::CreateRenderTarget() {
    if (!swap_chain_ || !d3d_device_) return;

    ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (SUCCEEDED(hr)) {
        d3d_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_);
    }
}

void D3D11Renderer::Resize(int width, int height) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    if (!swap_chain_) return;

    render_target_view_.Reset();
    width_  = width > 0 ? width : width_;
    height_ = height > 0 ? height : height_;

    swap_chain_->ResizeBuffers(0, static_cast<UINT>(width_), static_cast<UINT>(height_), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    CreateRenderTarget();
}

void D3D11Renderer::SetKeymapBadges(const KeymapBadge* badges, size_t count) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    badges_.clear();
    if (badges && count > 0) {
        badges_.assign(badges, badges + count);
    }
}

void D3D11Renderer::SetHudVisible(bool visible) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    hud_visible_ = visible;
}

void D3D11Renderer::SetFullscreen(bool fullscreen) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    if (!swap_chain_) return;
    swap_chain_->SetFullscreenState(fullscreen ? TRUE : FALSE, nullptr);
}

void D3D11Renderer::RenderFrame(const decoder::DecodedFrame& frame) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    if (!is_initialized_ || !d3d_context_ || !swap_chain_) return;

    has_received_frames_ = true;

    // Clear background to dark gaming black
    const float clear_color[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
    if (render_target_view_) {
        d3d_context_->ClearRenderTargetView(render_target_view_.Get(), clear_color);
    }

    if (frame.is_hardware_surface && frame.native_surface) {
        // ZERO-COPY GPU PRESENTATION:
        // Copy hardware decoded surface directly to back buffer when formats and dimensions match
        ComPtr<ID3D11Texture2D> back_buffer;
        if (SUCCEEDED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
            auto* src_tex = static_cast<ID3D11Texture2D*>(frame.native_surface);
            D3D11_TEXTURE2D_DESC src_desc{};
            src_tex->GetDesc(&src_desc);
            D3D11_TEXTURE2D_DESC bb_desc{};
            back_buffer->GetDesc(&bb_desc);

            if (src_desc.Format == bb_desc.Format && src_desc.Width == bb_desc.Width && src_desc.Height == bb_desc.Height) {
                d3d_context_->CopyResource(back_buffer.Get(), src_tex);
            } else if (src_desc.Format == bb_desc.Format) {
                UINT copy_w = std::min(src_desc.Width, bb_desc.Width);
                UINT copy_h = std::min(src_desc.Height, bb_desc.Height);
                D3D11_BOX box{ 0, 0, 0, copy_w, copy_h, 1 };
                d3d_context_->CopySubresourceRegion(back_buffer.Get(), 0, 0, 0, 0, src_tex, 0, &box);
            }
        }
    } else if (frame.raw_pixels != nullptr && frame.width > 0 && frame.height > 0) {
        // Software decoded buffer presentation
        ComPtr<ID3D11Texture2D> back_buffer;
        if (SUCCEEDED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
            D3D11_TEXTURE2D_DESC bb_desc{};
            back_buffer->GetDesc(&bb_desc);

            if (!software_texture_ || software_tex_w_ != bb_desc.Width || software_tex_h_ != bb_desc.Height) {
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = bb_desc.Width;
                desc.Height = bb_desc.Height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = bb_desc.Format;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
                if (SUCCEEDED(d3d_device_->CreateTexture2D(&desc, nullptr, &software_texture_))) {
                    software_tex_w_ = bb_desc.Width;
                    software_tex_h_ = bb_desc.Height;
                }
            }
            if (software_texture_) {
                UINT row_pitch = static_cast<UINT>(frame.width * 4);
                d3d_context_->UpdateSubresource(software_texture_.Get(), 0, nullptr, frame.raw_pixels, row_pitch, 0);
                d3d_context_->CopyResource(back_buffer.Get(), software_texture_.Get());
            }
        }
    }

    // Unthrottled low-latency present (SyncInterval = 0 for true 120Hz/240Hz refresh)
    swap_chain_->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

} // namespace unkxwyno::renderer
#endif // _WIN32
