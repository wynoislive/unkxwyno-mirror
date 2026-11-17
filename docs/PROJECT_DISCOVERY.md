# Project Discovery

Status: baseline recorded 2026-11-13. This document separates observed facts from design decisions and unresolved work.

## Observed

- The workspace was not a Git repository when the independent implementation began.
- The existing repository contains legacy `src/`, `test/`, `release/`, `ready-products/`, `refrence/`, and extensive reverse-engineering evidence under `docs/`.
- The two primary installer targets remain read-only:

  | Artifact | SHA-256 |
  | --- | --- |
  | `ready-products/DouWan-Video-Setup-En-4.4.0.9.exe` | `81a43b30558e34af6cb9c3056b07e8bc93d88ecb23e68257bb246da07f552f02` |
  | `ready-products/DouWan-Video-Setup-En-4.4.0.9-x64.exe` | `9d012d7452a4b2d65bf5046e3e97ad747e38110036c7aa86666ea23553b98476` |
- The tested legacy Windows wrapper at `release/unkxwyno_mirror.exe` has SHA-256 `8a827624296b38ab0f025ee539e71b7d8bee75e69f9c0e412fb83ca1b2246988`.
- That legacy wrapper is not an independent product. It depends on proprietary DouWan runtime components and is retained only as a tested reference.
- Installed host tools include Git, Java 17, and .NET SDK 8.0.426. .NET 10 desktop/runtime components are installed, but the .NET 10 SDK was not present at baseline.
- CMake, Ninja, a C/C++ compiler, Android SDK/ADB, Xcode, and Apple signing resources were not available at baseline.
- Existing static and runtime evidence records successful picture and audio through the former native USB path. It is historical behavior evidence, not proof for the new implementation.

## Decisions

- New implementation code lives in the new `apps/`, `core/`, `tests/`, `scripts/`, and `assets/` layout. Existing legacy paths remain untouched until an independently verified replacement exists.
- The new runtime will not load or redistribute DouWan binaries, use its activation system, or establish mirroring through AirPlay, Cast, or Miracast.
- Windows, macOS, Android, and iOS/iPadOS are first-class sender and receiver targets. Releases are gated independently by platform evidence.
- The product is permanently free: no advertisements, watermark, subscription, trial, paid quality tier, forced account, or artificial throttling.
- Runtime connectivity is USB or local network only. No cloud relay or telemetry service is part of the product.

## Unknown or unverified

- No independent capture, codec, discovery, transport, or render path had passed a build at this baseline.
- Apple source cannot be compiled or device-tested until a Mac and physical Apple devices are available.
- Android Open Accessory support and Windows driver packaging must be validated against real devices before USB support is advertised.
- iOS usbmux forwarding from the app capture lifecycle must pass a physical-device feasibility test.
- 4K/120 FPS is a capability target, not a current product claim.

## Evidence boundary

Prior REA reports in `docs/douwan-static-pass/`, `docs/douwan-architecture/`, and `docs/usb-investigation/` remain the evidence source for the legacy target. They must not be interpreted as new source code or as verification of the independent runtime.
