# Architecture

Status: approved design; implementation and platform verification are tracked separately in `BUILD_STATUS.md`.

## System shape

```text
Native capture -> native hardware encoder -> session/transport core
             -> authenticated local link -> jitter/decode -> native renderer
                                      \-> metrics and recovery controller
```

The shared C++20 core owns protocol types, capability negotiation, session state, queue limits, recovery, encoded-media routing, and metrics. Platform adapters retain ownership of raw GPU surfaces, capture permissions, hardware codecs, audio devices, and rendering. A versioned C ABI connects the core to native application layers.

## Platform adapters

| Platform | UI | Capture/audio | Codec/render |
| --- | --- | --- | --- |
| Windows | WinUI 3 / .NET 10 | Windows Graphics Capture and WASAPI | Media Foundation, D3D11/DirectComposition |
| macOS | SwiftUI | ScreenCaptureKit and CoreAudio | VideoToolbox, Metal, AVAudioEngine |
| Android | Kotlin/Compose | MediaProjection, AudioPlaybackCapture and microphone | MediaCodec, Surface, AudioTrack |
| iOS/iPadOS | SwiftUI | ReplayKit on iOS 17-26; ScreenCaptureKit on iOS 27+ | VideoToolbox, Metal, AVAudioEngine |

Platform APIs must be probed at runtime. A type existing in an SDK is not evidence that a device supports a requested stream profile.

## Shared modules

- `capture`: source descriptors and permission/capture contracts.
- `encoding`: codec capability descriptions and encoder/decoder contracts.
- `transport`: local wireless and wired byte/media transports.
- `discovery`: DNS-SD discovery plus manual-address fallback.
- `audio`: Opus transport, jitter handling, mixing, and A/V timing.
- `device`: identity, secure storage abstraction, and endpoint capabilities.
- `protocol`: versioned control messages and framing.
- `security`: pairing, peer authentication, and session resumption.
- `rendering`: native-renderer contracts and presentation statistics.
- `metrics`: requested-versus-actual quality, latency, drops, and resource use.

## Transport

Local network discovery uses DNS-SD with service type `_unkxwyno._tcp.local`. Signaling is a pinned TLS 1.3 connection. Wireless media uses host-only WebRTC/DTLS-SRTP; STUN, TURN, cloud signaling, and Internet fallback are disabled.

Companion wired media uses the same control and capability messages over a
reliable framed channel. Android Open Accessory and iOS usbmux forwarding remain
companion-transport designs. They do not reproduce native iPhone USB capture.

The user's current reconstruction priority follows the observed native path:
lockdown trust/session setup, Apple FF/2A/FF USB bulk capture, synchronization,
sample decoding and native rendering. Recovered framing and an independent
WinUSB transfer adapter live in `src/backend/independent`; capture-mode activation
and the full media pipeline remain incomplete. See the dated reconstruction
findings for evidence and differences from the companion design.

## Session lifecycle

`Idle -> Discovering -> Pairing -> Connecting -> Negotiating -> Streaming -> Recovering -> Stopped/Failed`

Heartbeats, reconnect backoff, queue bounds, and keyframe recovery belong to the shared state machine. A disconnect never allows unbounded encoded-frame accumulation.

## Quality semantics

Requested settings and active settings are separate values. Manual resolution/FPS remain user intent; adaptive bitrate may change within the selected profile. Auto mode may change resolution or FPS and must report each active value. A profile is enabled only after intersecting source, capture, encoder, transport, decoder, and display capabilities.

Unique frames are counted at capture, encode, decode, and presentation. Upscaling or repeating frames never qualifies as higher native resolution or FPS.
