# Engineering Decisions

## D-001: Independent clean-room runtime

The production runtime never loads or launches DouWan, AirPlay, Cast, or
Miracast components. Existing artifacts are preserved only as hash-bound
behavioral evidence.

## D-002: Native platform media adapters

Each operating system owns capture, hardware codec, audio, and rendering.
Portable code owns protocol, negotiation, recovery, queues, and statistics.

## D-003: Honest capability negotiation

Requested settings remain distinct from active settings. Unsupported profiles
carry a machine-readable limiting stage, user-facing explanation, and highest
valid alternative. Upscaling or duplicate frames never count as 4K or 120 FPS.

## D-004: Local-only communication

Discovery and streaming use USB or the local network. The runtime has no cloud
relay, telemetry, ad, account, or automatic Internet-update endpoint.

## D-005: Per-platform release gates

Platforms share one protocol but release independently after real hardware
validation. Apple status remains blocked until Mac hardware is available.

## D-006: Permanent free product

No capability may depend on payment, licensing state, advertisements,
registration, session duration, resolution, or frame-rate tier.

## D-007: Reconstruct native capture without restoring the original engine

The 2026-11-14 instruction rejects restoring the previous native-component
package. Reconstruct observed wired/wireless behavior in independent source.
Do not describe the companion usbmux tunnel as the recovered USB screen-capture
mechanism. Native capture uses the observed dedicated bulk interface; lockdown
pairing/session initialization is a separate layer. Wireless protocol selection
must follow actual evidence, including AirPlay/RAOP indications, and cannot
silently substitute a custom companion protocol. Existing prohibitions on
protected-data extraction and authentication/DRM bypass remain in force.

