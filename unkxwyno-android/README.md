# unkXwyno Mirror - Android Companion App

[![Android](https://img.shields.io/badge/Platform-Android%208.0%2B-brightgreen.svg)](https://unkxwyno-mirror.genzhost.best)
[![Kotlin](https://img.shields.io/badge/Kotlin-2.0.0-blue.svg)](https://kotlinlang.org)
[![AOA 2.0](https://img.shields.io/badge/Protocol-AOA%202.0%20Zero--Debugging-purple.svg)](https://unkxwyno-mirror.genzhost.best)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> **Official Download & Documentation**: [https://unkxwyno-mirror.genzhost.best](https://unkxwyno-mirror.genzhost.best)  
> **Package Identifier**: `best.genzhost.unkxwynomirror`

---

## ⚡ Overview

`unkxwyno-android` is the companion sender application for the **unkXwyno Mirror** ecosystem. It provides hardware-accelerated, sub-millisecond screen and internal audio capture over wired USB without requiring Developer Options, USB Debugging (ADB), or root privileges.

### Key Capabilities

- **Zero-Debugging AOA 2.0 Transport**: Direct USB Accessory protocol (`android.hardware.usb.action.USB_ACCESSORY_ATTACHED`) providing raw kernel-level bidirectional bulk pipe (`FileInputStream` / `FileOutputStream`).
- **Low-Latency Hardware Video Encoder**: `MediaCodec` AVC/H.264 pipeline using `COLOR_FormatSurface` with CBR bitrate control, realtime thread priority, and `KEY_LATENCY = 0` (Android 11+).
- **Internal Audio Capture (API 29+)**: High-fidelity 48kHz 16-bit stereo PCM capture via `AudioPlaybackCaptureConfiguration` with zero audio degradation or microphone ambient noise.
- **Sub-1ms Hardware Touch Backchannel**: Direct kernel-level digitizer events received over AOA 2.0 HID descriptors (`Usage Page 0x0D`, `Usage 0x04`).
- **Dual Compatibility**: Filter configuration supporting both native `unkXwyno` hosts and legacy `XinDawn` accessories.

---

## 📂 Project Architecture

```
unkxwyno-android/
├── app/
│   ├── src/main/
│   │   ├── AndroidManifest.xml             # Foreground service & USB Accessory filters
│   │   ├── res/
│   │   │   ├── xml/accessory_filter.xml    # AOA 2.0 accessory matching filters
│   │   │   └── values/                     # Strings, dark colors, and themes
│   │   └── kotlin/best/genzhost/unkxwynomirror/
│   │       ├── MainActivity.kt             # Jetpack Compose dark UI & permissions
│   │       ├── service/
│   │       │   └── MirroringService.kt     # Foreground Service managing streaming lifecycle
│   │       ├── encoder/
│   │       │   └── VideoEncoder.kt         # MediaCodec low-latency H.264 CBR encoder
│   │       ├── audio/
│   │       │   └── AudioCaptureManager.kt  # AudioPlaybackCapture 48kHz PCM capture
│   │       ├── usb/
│   │       │   └── AoaAccessoryManager.kt  # UsbAccessory endpoint connection manager
│   │       ├── protocol/
│   │       │   └── PacketFraming.kt        # 8-byte LE packet framing serializer
│   │       └── ui/theme/
│   │           └── Theme.kt                # Cyan/Purple cyberpunk Material 3 theme
│   └── build.gradle.kts                    # App dependencies & SDK versions (minSdk 26, compileSdk 35)
├── settings.gradle.kts
├── build.gradle.kts
└── gradle.properties
```

---

## 🚀 Building & Installation

### Requirements
- Android SDK 35
- JDK 17+
- Gradle 8.5+

### Build APK
```bash
./gradlew assembleRelease
```
The output APK will be generated at `app/build/outputs/apk/release/app-release.apk`.

---

## 📄 License

Open source under the [MIT License](LICENSE). Copyright (c) 2026 unkXwyno.
