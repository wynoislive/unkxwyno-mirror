# Compatibility

Status: target matrix, not a claim of completed support.

## Operating systems

| Platform | Initial target | Current verification |
| --- | --- | --- |
| Windows | Windows 11 24H2+, x64 and Arm64 | Not verified independently |
| macOS | macOS 14+ | Blocked: no Mac |
| Android | Android 12 / API 31+ | SDK and device validation pending |
| iPhone/iPad | iOS/iPadOS 17+ | Companion app blocked: no Mac/signing environment. Native Windows USB trial with iOS 18.7.11 reaches Pair/StartSession but fails TLS; mirroring is not accepted. |

## Connection matrix

| Pair | Local Wi-Fi | Direct/application USB |
| --- | --- | --- |
| Windows <-> macOS | Planned | Only through an OS-provided network interface |
| Windows/macOS <-> Android | Planned | Planned through Android Open Accessory |
| Windows/macOS <-> iOS/iPadOS | Planned | Windows native capture integration partial; USBMux control works, live trusted TLS/media fail acceptance. Companion forwarding remains planned. |
| Android <-> iOS/iPadOS | Planned | Unsupported in v1: no universal public app channel |
| Phone <-> same phone platform | Planned between separate devices | Unsupported in v1 |

A supported connection is bidirectional at the application layer: either endpoint may send or receive after capability negotiation. Cable orientation does not imply capture permission.

## Media profiles

H.264 is the mandatory planned interoperability codec. HEVC and AV1 are optional and require encoder and decoder support at both endpoints. Opus 48 kHz is the planned audio transport codec. Protected media, app-opted-out audio, and hardware limitations are reported rather than bypassed.

Resolution choices are 480p, 720p, 1080p, 1440p, 2160p, Native, Auto, and Custom. FPS choices are 24, 30, 45, 60, 90, 120, and Auto. Availability depends on the full runtime pipeline and is never unlocked by payment.
