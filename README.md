# UNKXWYNO Mirror

UNKXWYNO Mirror is a local-first, independent, clean-room screen-mirroring platform for Windows, macOS, Android, iPhone, and iPad. Authored source is MIT licensed and the complete product is permanently free: no ads, no watermarks, no accounts, no subscriptions, no paid quality tiers, and no artificial session limits.

---

## Latest Windows Build

The latest verified development release package is located at:
```text
out/windows-x64-usb-latency-20261115-v4/
```
The main application executable is:
```powershell
out\windows-x64-usb-latency-20261115-v4\UnkxwynoMirror.Desktop.exe
```

### Windows Runtime Prerequisites
- **Operating System**: Windows 10 (version 1809+ / Build 17763+) or Windows 11 (x64)
- **.NET 10 Runtime**: [.NET 10 Desktop Runtime (x64)](https://dotnet.microsoft.com/download/dotnet/10.0)
- **Windows App Runtime**: [Windows App SDK 1.6+ Runtime (x64)](https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads)

---

## Quickstart: How to Use

### 1. iPhone / iPad Wired USB Mirroring
UNKXWYNO Mirror features an independent, clean-room implementation of the QuickTime USB screen-capture protocol and USBMux pairing over USB bulk endpoints:

1. **Connect Device**: Connect your iPhone or iPad to your PC using a reliable USB data cable.
2. **Unlock & Trust**: Unlock the iOS device and tap **"Trust This Computer"** when prompted.
3. **Launch UNKXWYNO Mirror**:
   ```powershell
   .\out\windows-x64-usb-latency-20261115-v4\UnkxwynoMirror.Desktop.exe
   ```
4. **Configure Settings**:
   - Select your target resolution (e.g., `1080p (1920 × 1080)` or `Native device resolution`).
   - Select target frame rate cadence (`60 FPS` or `Auto`).
5. **Start USB Mirror**:
   - Click the **"Mirror iPhone USB"** button in the dashboard.
   - **Driver Authorization**: If prompted by Windows UAC (User Account Control), click **Yes**. UNKXWYNO uses an elevated lease (`UsbDriverSession.ps1`) to temporarily acquire the capture driver (`libusb0` / `WinUSB`) and **guarantees full restoration** back to the default Apple composite driver (`usbccgp / oem14.inf`) upon session exit without requiring a reboot.
6. **Live Viewer Window**:
   - A dedicated low-latency hardware/software viewer window opens titled `UNKXWYNO Mirror - iPhone USB`.
   - Includes standard window controls (Minimize, Maximize/Restore, Fit/Actual size, Close) without distracting DWM accent borders.
   - The status bar displays real-time telemetry:
     - Target FPS request cadence
     - Device PTS (Presentation Time Stamp) FPS
     - Displayed presentation FPS
     - SIMD NV12 color conversion time (ms)
     - Host USB sample-to-paint latency (ms)
     - Replaced-frame count
7. **Stop Mirroring**: Close the viewer window or click **"Stop Mirroring"** in the main application. The driver lease automatically closes and restores the default Apple driver.

### 2. Local Network (Wi-Fi) Discovery
- Ensure both your PC and companion devices are on the same local network / Wi-Fi subnet.
- The desktop application automatically binds to local IPv4 mDNS (`_unkxwyno._tcp.local`) to discover peers without sending traffic to any external cloud servers.
- When an independent companion device is detected, approve the connection pairing to start wireless streaming.

---

## Technical Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                   UNKXWYNO Desktop (WinUI 3 / .NET 10)                 │
│  - SettingsStore (atomic JSON)      - LocalDiscoveryService (mDNS)     │
│  - UsbDriverLease (safe rollback)   - UsbTrustedSession (mTLS/DPAPI)   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ C ABI (UXM C-API v1)
┌───────────────────────────────────▼────────────────────────────────────┐
│                  Core Engine (C++20 unkxwyno_core.dll)                 │
│  ┌───────────────────────┐  ┌────────────────────────────────────────┐ │
│  │ USB Capture Protocol  │  │ Decoupled Presentation Pipeline        │ │
│  │ - Bulk endpoints      │  │ - Windows Media Foundation H.264 (MFT) │ │
│  │ - USBMux v1/v2        │  │ - AVX2 / SSE2 SIMD NV12->BGRA convert  │ │
│  │ - Paced NEED pump     │  │ - 1-frame bounded async decode worker  │ │
│  │ - OpenSSL 3 mTLS      │  │ - Flicker-free letterbox GDI viewer    │ │
│  └───────────────────────┘  └────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```

- **Clean-room Protocol Implementation**: No proprietary vendor receiver DLLs, AirPlay reverse-engineering bypasses, or third-party proprietary dependencies.
- **Ultra-low Host Render Latency**:
  - Run-time vectorized **AVX2** color space conversion (2.7 ms per 1080p frame, 3.6x faster than scalar algorithms) with SSE2 and scalar fallbacks.
  - Decoupled worker thread holding at most one compressed frame to prevent queue backlog while keeping frame ordering intact.
  - Immutable shared frame snapshots prevent rendering paints (`StretchDIBits`) from blocking incoming USB packet parsing.
  - Flicker-free presentation retains previous frames and clears only black letterbox bars.
- **Security & Privacy**:
  - Zero cloud telemetry, advertising, or remote update endpoints.
  - Pair records and identity keys are generated locally and encrypted using Windows DPAPI.
  - Automatic driver restoration journal ensures driver state is cleanly restored even after unexpected process termination.

---

## Repository Layout

- `apps/desktop`: Windows WinUI 3 desktop application, UI models, and USB session managers.
- `apps/android`: Android companion app scaffold (MediaProjection, NSD discovery, CodecProbe).
- `apps/ios`: iOS companion library scaffold (Screen capture broadcast picker, Network framework).
- `apps/macos`: macOS companion library scaffold (ScreenCaptureKit, AVFoundation).
- `core`: Portable C++20 core engine (session state, capability negotiation, UXM1 framing, metrics).
- `src/backend/independent`: Clean-room USB capture protocol, transports (WinUSB, libusb, libusb-win32), SIMD color conversion, and viewer window.
- `tests`: Test suites covering core CTest, desktop contracts, USB protocol boundaries, and package audits.
- `scripts`: Reproducible build, test, package verification, and publish automation.
- `docs`: Architecture specifications, protocols, security model, and latency test findings.
- `out/windows-x64-usb-latency-20261115-v4`: Latest complete x64 standalone Windows application package.

---

## Building from Source

### Prerequisites
1. **.NET 10 SDK** (version 10.0.100+)
2. **CMake** (version 3.20+) and **Ninja** (version 1.11+)
3. **C++20 Compiler**: LLVM-MinGW (UCRT) or MSVC C++20 compiler
4. **PowerShell 7+** (or Windows PowerShell 5.1)

### Build Commands

1. **Build All Components (Debug / Release)**:
   ```powershell
   ./scripts/build.ps1 -Configuration Release -Component All
   ```

2. **Run All Automated Tests**:
   ```powershell
   ./scripts/test.ps1 -Component All
   ```
   This executes:
   - Independent source code audits (zero forbidden legacy/monetization keywords).
   - .NET desktop contract tests and settings integrity tests.
   - C++20 core unit tests via CTest (`unkxwyno_core_tests`).
   - USB capture protocol, SIMD conversion, and H.264 fixture decode tests (`unkxwyno_usb_capture_tests`).
   - Mock and live USB driver resolution tests (`UsbDriverLookup.Tests.ps1`).
   - Mutual TLS and USB pairing tests (`UnkxwynoMirror.UsbPairing.Tests.csproj`).

3. **Publish Windows Desktop Package**:
   ```powershell
   ./scripts/publish-windows.ps1 -Configuration Release -OutputPath out/windows-x64-custom
   ```
   The publish script compiles all WinUI XBF/PRI resources, gathers OpenSSL and libusb dependencies, and verifies the output package against the 59-file integrity audit.

---

## Current Status & Verification

| Component | Status | Details |
| :--- | :--- | :--- |
| **Windows Desktop UI** | ✅ Verified | WinUI 3, responsive layout, settings persistence, diagnostics logging |
| **C++20 Core & UXM1** | ✅ Verified | Unit tests pass 2/2 in Debug and Release; C ABI verified |
| **SIMD NV12 Converter** | ✅ Verified | AVX2/SSE2 verified byte-for-byte against reference equations |
| **H.264 Windows Decoder** | ✅ Verified | Low-latency MFT decoder tested with captured H.264 streams |
| **Wired iPhone Mirroring**| ⚠️ Beta / Active Development | Live 832×1792 video renders smoothly; long-session USB stability and hardware sync are actively being optimized |
| **Driver Lease Broker** | ✅ Verified | Automatic libusb lease with guaranteed Apple driver rollback |
| **Android / Apple Companions** | 🚧 Scaffolding | Native platform scaffolds created; cross-platform validation in progress |

For detailed experimental measurements, review [docs/USB_LATENCY_FINDINGS.md](docs/USB_LATENCY_FINDINGS.md) and [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

---

## Product Policy & License

All implemented features, frame rates, and resolutions are free of charge. UNKXWYNO Mirror never charges for capabilities, displays ads, injects watermarks, or enforces time limits.

Authored source code is licensed under the **[MIT License](LICENSE)**.
Third-party component notices and licenses (OpenSSL, libusb) are documented in **[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)**.
