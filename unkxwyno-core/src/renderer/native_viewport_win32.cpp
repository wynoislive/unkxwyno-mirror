/**
 * @file native_viewport_win32.cpp
 * @brief Dedicated High-Priority Win32 Thread Native Borderless Viewport Window.
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
#include <thread>
#include <atomic>
#include <future>
#include <iostream>

namespace unkxwyno::renderer {

#define WM_USER_SHOW       (WM_USER + 101)
#define WM_USER_FULLSCREEN (WM_USER + 102)
#define WM_USER_CLOSE      (WM_USER + 103)

class Win32NativeViewport final : public INativeViewport {
public:
    Win32NativeViewport() = default;

    ~Win32NativeViewport() override {
        Close();
    }

    bool Create(const std::string& title, int width, int height) override {
        if (is_open_) return true;

        title_ = title;
        target_width_  = width > 0 ? width : 1920;
        target_height_ = height > 0 ? height : 1080;

        std::promise<bool> init_promise;
        auto init_future = init_promise.get_future();

        viewport_thread_ = std::thread(&Win32NativeViewport::ViewportThreadProc, this, std::ref(init_promise));

        bool ok = init_future.get();
        if (!ok) {
            if (viewport_thread_.joinable()) viewport_thread_.join();
            return false;
        }

        is_open_ = true;
        return true;
    }

    void Show(bool visible) override {
        if (hwnd_) {
            PostMessageW(hwnd_, WM_USER_SHOW, visible ? 1 : 0, 0);
        }
    }

    void SetFullscreen(bool fullscreen) override {
        if (hwnd_) {
            PostMessageW(hwnd_, WM_USER_FULLSCREEN, fullscreen ? 1 : 0, 0);
        }
    }

    void Close() override {
        if (hwnd_) {
            PostMessageW(hwnd_, WM_USER_CLOSE, 0, 0);
            if (viewport_thread_.joinable()) {
                viewport_thread_.join();
            }
            hwnd_ = nullptr;
        }
        is_open_ = false;
    }

    [[nodiscard]] void* GetNativeHandle() const override {
        return hwnd_.load();
    }

    [[nodiscard]] bool IsOpen() const override {
        return is_open_.load();
    }

    void GetWindowBounds(int& out_x, int& out_y, int& out_w, int& out_h) const override {
        if (hwnd_) {
            RECT rc{};
            GetWindowRect(hwnd_, &rc);
            out_x = rc.left;
            out_y = rc.top;
            out_w = rc.right - rc.left;
            out_h = rc.bottom - rc.top;
        } else {
            out_x = 0; out_y = 0; out_w = 0; out_h = 0;
        }
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

    void SetMoveCallback(MoveCallback callback) override {
        move_callback_ = std::move(callback);
    }

    void PollEvents() override {
        // Dedicated thread continuously handles messages via GetMessageW.
    }

private:
    void ViewportThreadProc(std::promise<bool>& init_promise) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        HINSTANCE hInstance = GetModuleHandleW(nullptr);
        const wchar_t CLASS_NAME[] = L"unkXwyno_Mirror_Viewport";

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = StaticWndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
        wc.hbrBackground = nullptr; // Null brush so Windows GDI never clears or paints white over D3D11 swapchain
        wc.lpszClassName = CLASS_NAME;

        RegisterClassExW(&wc);

        std::wstring title_w(title_.begin(), title_.end());

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        int pos_x = (screen_w - target_width_) / 2;
        int pos_y = (screen_h - target_height_) / 2;

        HWND hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            CLASS_NAME,
            title_w.c_str(),
            WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
            pos_x, pos_y, target_width_, target_height_,
            nullptr, nullptr, hInstance, this
        );

        if (!hwnd) {
            init_promise.set_value(false);
            return;
        }

        hwnd_.store(hwnd);
        width_ = target_width_;
        height_ = target_height_;
        init_promise.set_value(true);

        // Standard Win32 Message Loop on dedicated thread
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        hwnd_.store(nullptr);
    }

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
            case WM_USER_SHOW:
                ShowWindow(hwnd, wParam ? SW_SHOW : SW_HIDE);
                return 0;

            case WM_USER_FULLSCREEN: {
                bool fs = (wParam != 0);
                if (fs && !is_fullscreen_) {
                    GetWindowRect(hwnd, &saved_rect_);
                    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                    int sw = GetSystemMetrics(SM_CXSCREEN);
                    int sh = GetSystemMetrics(SM_CYSCREEN);
                    SetWindowPos(hwnd, HWND_TOP, 0, 0, sw, sh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    is_fullscreen_ = true;
                } else if (!fs && is_fullscreen_) {
                    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME);
                    SetWindowPos(hwnd, HWND_NOTOPMOST,
                                 saved_rect_.left, saved_rect_.top,
                                 saved_rect_.right - saved_rect_.left,
                                 saved_rect_.bottom - saved_rect_.top,
                                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    is_fullscreen_ = false;
                }
                return 0;
            }

            case WM_USER_CLOSE:
                DestroyWindow(hwnd);
                return 0;

            case WM_MOVE: {
                int x = static_cast<short>(LOWORD(lParam));
                int y = static_cast<short>(HIWORD(lParam));
                if (move_callback_) {
                    move_callback_(x, y, width_, height_);
                }
                return 0;
            }

            case WM_SIZE: {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                if (w > 0 && h > 0) {
                    width_ = w;
                    height_ = h;
                    if (resize_callback_) resize_callback_(w, h);
                    if (move_callback_) {
                        RECT rc{};
                        GetWindowRect(hwnd, &rc);
                        move_callback_(rc.left, rc.top, w, h);
                    }
                }
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

                if (width_ > 0 && height_ > 0) {
                    uint16_t norm_x = static_cast<uint16_t>((std::clamp(mouse_x, 0, width_) * 65535) / width_);
                    uint16_t norm_y = static_cast<uint16_t>((std::clamp(mouse_y, 0, height_) * 65535) / height_);
                    touch_callback_(0, action, norm_x, norm_y, is_down ? 800 : 0);
                }
                return 0;
            }

            case WM_KEYDOWN:
            case WM_KEYUP: {
                if (key_callback_) {
                    bool is_down = (uMsg == WM_KEYDOWN);
                    uint16_t vkey = static_cast<uint16_t>(wParam);
                    uint8_t mods = 0;
                    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= 1;
                    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= 2;
                    if (GetKeyState(VK_MENU) & 0x8000) mods |= 4;
                    key_callback_(vkey, is_down, mods);
                }
                return 0;
            }

            case WM_ERASEBKGND:
                // Return 1: Direct3D 11 handles all background rendering to avoid white flashes
                return 1;

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            default:
                break;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    std::string title_;
    int target_width_ = 1920;
    int target_height_ = 1080;
    int width_ = 1920;
    int height_ = 1080;

    std::atomic<HWND> hwnd_{nullptr};
    std::atomic<bool> is_open_{false};
    bool is_fullscreen_ = false;
    RECT saved_rect_{};

    std::thread viewport_thread_;

    TouchInputCallback touch_callback_;
    KeyInputCallback   key_callback_;
    ResizeCallback     resize_callback_;
    MoveCallback       move_callback_;
};

std::unique_ptr<INativeViewport> CreatePlatformViewport() {
    return std::make_unique<Win32NativeViewport>();
}

} // namespace unkxwyno::renderer

#endif // _WIN32
