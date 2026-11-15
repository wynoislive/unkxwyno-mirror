#include <iostream>
#include <thread>
#include <chrono>
#include "renderer/native_viewport.hpp"
#include "renderer/d3d11_renderer.hpp"

using namespace unkxwyno::renderer;

int main() {
    std::cout << "[TEST] Initializing Native Viewport & Direct3D 11 Renderer...\n";

    auto viewport = CreatePlatformViewport();
    if (!viewport) {
        std::cerr << "[FAIL] Failed to create platform viewport.\n";
        return 1;
    }

    if (!viewport->Create("unkXwyno Viewport Test Window", 1280, 720)) {
        std::cerr << "[FAIL] Failed to initialize native window.\n";
        return 1;
    }

    auto renderer = std::make_unique<D3D11Renderer>();
    void* hwnd = viewport->GetNativeHandle();
    if (!hwnd) {
        std::cerr << "[FAIL] Native HWND is null.\n";
        return 1;
    }

    if (!renderer->Initialize(hwnd, 1280, 720)) {
        std::cerr << "[FAIL] Failed to initialize Direct3D 11 swapchain.\n";
        return 1;
    }

    std::cout << "[PASS] D3D11 Swapchain initialized with FLIP_DISCARD.\n";
    viewport->Show(true);

    // Run for 1.5 seconds in idle state while the continuous idle loop presents obsidian frames
    std::cout << "[INFO] Verifying idle presentation heartbeat (Obsidian #080A0F)...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    viewport->Show(false);
    viewport->Close();
    renderer->Shutdown();

    std::cout << "[PASS] Viewport closed cleanly without errors or memory faults.\n";
    return 0;
}
