# unkXwyno Mirror - Desktop Control Dock & Game Keymapper

[![.NET 9](https://img.shields.io/badge/.NET-9.0-purple.svg)](https://dotnet.microsoft.com/)
[![Avalonia UI](https://img.shields.io/badge/UI-Avalonia%2011.1-blue.svg)](https://avaloniaui.net/)
[![P/Invoke](https://img.shields.io/badge/Interop-Zero--Copy%20C%20ABI-green.svg)](https://unkxwyno-mirror.genzhost.best)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> **Official Download & Documentation**: [https://unkxwyno-mirror.genzhost.best](https://unkxwyno-mirror.genzhost.best)

---

## ⚡ Overview

`unkxwyno-desktop` is the cross-platform presentation dock and keymapper overlay for the **unkXwyno Mirror** suite. Built with **C# .NET 9** and **Avalonia UI**, it operates as a floating, semi-transparent control center that orchestrates the native C++20 bare-metal mirroring core (`unkxwyno-core`).

### Key Capabilities

- **Zero-Copy C ABI P/Invoke**: Connects directly to `unkxwyno_core.dll` without COM wrappers or managed marshaling overhead.
- **Floating Glassmorphic Dock**: Draggable acrylic/mica toolbar providing real-time FPS, bitrate, hardware decode latency, and audio jitter telemetry.
- **Independent High-Refresh Viewport**: Decoupled Win32 / Cocoa / X11 borderless viewport running exclusive `FLIP_DISCARD` Direct3D 11 presentation up to 240 Hz.
- **Mobile Game Keymapper**: Transparent overlay allowing players to map WASD, Mouse Fire (L-Click), Scope (R-Click), and custom keybinds directly to sub-1ms AOA 2.0 multi-touch hardware HID reports.
- **Cross-Platform**: Ready for Windows 10/11, Linux (X11/Wayland), and macOS.

---

## 📂 Project Architecture

```
unkxwyno-desktop/
├── Interop/
│   └── UnkxwynoCore.cs             # P/Invoke C ABI declarations (Engine, Viewport, Input, Metrics)
├── Models/
│   └── KeymapProfile.cs            # Keymapping configuration models & Battle Royale presets
├── Services/
│   └── MirrorEngineService.cs      # Engine lifecycle, telemetry polling, and safe memory management
├── ViewModels/
│   └── MainViewModel.cs            # MVVM state holder (CommunityToolkit.Mvvm)
├── Views/
│   ├── MainWindow.axaml            # Glassmorphic dock XAML
│   ├── MainWindow.axaml.cs         # Dragging and window management
│   ├── KeymapperOverlayWindow.axaml# Gaming keymapper visual canvas
│   └── KeymapperOverlayWindow.axaml.cs # Live key-to-touch dispatching
├── App.axaml                       # Global Fluent Dark Theme and unkXwyno palette
├── Program.cs                      # Entry point with headless smoke test support
└── unkxwyno-desktop.csproj         # Target net9.0 with Avalonia 11.1
```

---

## 🚀 Building & Running

### Requirements
- [.NET 9.0 SDK](https://dotnet.microsoft.com/download/dotnet/9.0)
- Native `unkxwyno_core.dll` (built from `unkxwyno-core/`)

### Build Release
```bash
dotnet build -c Release
```

### Run Smoke Test (Headless)
```bash
dotnet run -c Release -- --smoke-test
```

### Launch Desktop Application
```bash
dotnet run -c Release
```

---

## 📄 License

Open source under the [MIT License](LICENSE). Copyright (c) 2026 unkXwyno.
