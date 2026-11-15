/**
 * @file native_viewport_win32.cpp
 * @brief Win32 native borderless viewport window implementation.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

#include "native_viewport.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <iostream>

namespace unkxwyno::renderer {

class Win32NativeViewport final : public INativeViewport {
public:
    Win32NativeViewport() = default;
    ~Win32NativeViewport() override {
        Close();
    }

    bool Create(const std::string& title, int width, int height) override {
        HINSTANCE hInstance = GetModuleHandleW(nullptr);
        const wchar_t CLASS_NAME[] = L"unkXwyno_Mirror_Viewport";

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = StaticWndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = CLASS_NAME;

        RegisterClassExW(&wc);

        std::wstring title_w(title.begin(), title.end());

        // Center on primary monitor
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        int pos_x = (screen_w - width) / 2;
        int pos_y = (screen_h - height) / 2;

        hwnd_ = CreateWindowExW(
            WS_EX_APPWINDOW,
            CLASS_NAME,
            title_w.c_str(),
            WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
            pos_x, pos_y, width, height,
            nullptr, nullptr, hInstance, this
        );

        if (!hwnd_) return false;

        width_  = width;
        height_ = height;
        is_open_ = true;
        return true;
    }

    void Show(bool visible) override {
        if (hwnd_) {
            ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
        }
    }

    void SetFullscreen(bool fullscreen) override {
        if (!hwnd_) return;
        if (fullscreen && !is_fullscreen_) {
            GetWindowRect(hwnd_, &saved_rect_);
            SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            int screen_w = GetSystemMetrics(SM_CXSCREEN);
            int screen_h = GetSystemMetrics(SM_CYSCREEN);
            SetWindowPos(hwnd_, HWND_TOP, 0, 0, screen_w, screen_h, SWP_FRAMECHANGED);
            is_fullscreen_ = true;
        } else if (!fullscreen && is_fullscreen_) {
            SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME);
            SetWindowPos(hwnd_, HWND_NOTOPMOST,
                         saved_rect_.left, saved_rect_.top,
                         saved_rect_.right - saved_rect_.left,
                         saved_rect_.bottom - saved_rect_.top,
                         SWP_FRAMECHANGED);
            is_fullscreen_ = false;
        }
    }

    void Close() override {
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        is_open_ = false;
    }

    [[nodiscard]] void* GetNativeHandle() const override {
        return hwnd_;
    }

    [[nodiscard]] bool IsOpen() const override {
        return is_open_;
    }

    void SetTouchCallback(TouchInputCallback callback) override {
        touch_callback_ = std::move(callback);
    }

    void SetKeyCallback(KeyInputCallback callback) override {
        key_callback_ = std::move(callback);
    }

    void SetResizeCallback(ResizeCallback callback) override {
        resize_callback_ = std::move(callback);
    }

    void PollEvents() override {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        Win32NativeViewport* self = nullptr;
        if (uMsg == WM_NCCREATE) {
            auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<Win32NativeViewport*>(create_struct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<Win32NativeViewport*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self) {
            return self->HandleMessage(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_SIZE: {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                width_ = w;
                height_ = h;
                if (resize_callback_) resize_callback_(w, h);
                return 0;
            }

            case WM_LBUTTONDOWN:
            case WM_MOUSEMOVE:
            case WM_LBUTTONUP: {
                if (!touch_callback_) break;
                bool is_down = (wParam & MK_LBUTTON) != 0;
                uint8_t action = UNKXWYNO_TOUCH_MOVE;
                if (uMsg == WM_LBUTTONDOWN) action = UNKXWYNO_TOUCH_DOWN;
                else if (uMsg == WM_LBUTTONUP) action = UNKXWYNO_TOUCH_UP;

                int mouse_x = GET_X_LPARAM(lParam);
                int mouse_y = GET_Y_LPARAM(lParam);

                if (width_ > 0 && height_ > 0 && (is_down || action == UNKXWYNO_TOUCH_UP)) {
                    uint16_t norm_x = static_cast<uint16_t>((std::max(0, std::min(mouse_x, width_)) * 65535) / width_);
                    uint16_t norm_y = static_cast<uint16_t>((std::max(0, std::min(mouse_y, height_)) * 65535) / height_);
                    touch_callback_(0, action, norm_x, norm_y, is_down ? 512 : 0);
                }
                return 0;
            }

            case WM_KEYDOWN:
            case WM_KEYUP: {
                if (key_callback_) {
                    bool is_key_down = (uMsg == WM_KEYDOWN);
                    uint16_t key_code = static_cast<uint16_t>(wParam);
                    uint8_t modifiers = 0;
                    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= 1;
                    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= 2;
                    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= 4;
                    key_callback_(key_code, is_key_down, modifiers);
                }
                return 0;
            }

            case WM_CLOSE:
                is_open_ = false;
                ShowWindow(hwnd, SW_HIDE);
                return 0;

            case WM_DESTROY:
                is_open_ = false;
                hwnd_ = nullptr;
                return 0;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    HWND hwnd_{nullptr};
    int width_{1920};
    int height_{1080};
    bool is_open_{false};
    bool is_fullscreen_{false};
    RECT saved_rect_{};

    TouchInputCallback touch_callback_;
    KeyInputCallback key_callback_;
    ResizeCallback resize_callback_;
};

std::unique_ptr<INativeViewport> CreatePlatformViewport() {
    return std::make_unique<Win32NativeViewport>();
}

} // namespace unkxwyno::renderer
#endif // _WIN32
