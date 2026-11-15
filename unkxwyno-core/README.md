# unkxwyno-core

> **Ultra-Low-Latency Native C++20 Core Mirroring Engine**  
> **Official Web**: [https://unkxwyno-mirror.genzhost.best](https://unkxwyno-mirror.genzhost.best)

`unkxwyno-core` is the native zero-dependency engine powering screen mirroring, audio streaming, and sub-1ms input injection for **unkXwyno Mirror**.

---

## 🚀 Key Architectural Pillars

- **Zero External Runtime Dependencies**: No `adb.exe`, no iTunes / Apple Mobile Device Support, no FFmpeg.
- **Raw OS USB Engine**:
  - Windows: Asynchronous `WinUSB` with I/O Completion Ports (IOCP).
  - macOS: `IOKit/IOUSBHost`.
  - Linux: Raw asynchronous `usbfs` ioctls.
- **Hardware-Accelerated Video Decoders**:
  - Windows: Microsoft Media Foundation D3D11VA (`IMFTransform`) directly bound to `FLIP_DISCARD` DXGI swapchains.
  - macOS: Apple `VideoToolbox` decompression session directly rendered into `CAMetalLayer`.
  - Linux: VA-API (`libva`) / Vulkan swapchains.
- **Low-Latency Audio Engine**:
  - Windows WASAPI `IAudioClient3` (~3ms hardware device period).
  - macOS CoreAudio HAL with lock-free SPSC ring buffers and micro-resampling drift correction.
- **Sub-1ms Hardware Touch Injection**:
  - Android: AOAv2 USB HID multi-touch digitizer (`0x0D`/`0x04`) and keyboard.
  - iOS: PC Bluetooth LE HID peripheral module (GATT HOGP).
