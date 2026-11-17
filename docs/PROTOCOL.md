# UNKXWYNO Local Mirroring Protocol

Status: draft v1 contract. It is not yet an interoperability claim.

## Principles

- Local-only operation with authenticated peers.
- Explicit protocol major/minor version and forward-compatible control fields.
- Requested quality is never confused with negotiated or measured quality.
- Media queues are bounded; stale video is dropped before latency can grow without limit.
- No command provides remote keyboard/touch injection in v1.

## Discovery and pairing

Endpoints advertise `_unkxwyno._tcp.local` with protocol major, endpoint class, role flags, and an ephemeral instance identifier. No pairing secret appears in DNS-SD data.

Initial pairing establishes TLS 1.3 to a fingerprint supplied by QR or confirmed short authentication string. A one-time 256-bit token expires after use or a short security window. Successful pairing stores peer identity in the operating-system secure store. Later signaling uses mutual peer authentication.

## Control messages

The versioned schema contains at least:

- `Hello` and `DeviceCapabilities`
- `SessionOffer`, `SessionAccept`, and `SessionReject`
- `QualityRequest` and `NegotiatedProfile`
- `StreamStart`, `StreamReconfigure`, `KeyframeRequest`, and `StreamStop`
- `Heartbeat`, `SessionStats`, `Recover`, `Error`, and `Goodbye`

Every error includes a stable code, failing stage, retryability, human-readable explanation, and highest known alternative when applicable.

## Capability negotiation

The sender reports capture sources and encoder/audio capabilities. The receiver reports decoder, renderer, audio sink, display, and transport capabilities. The negotiated profile is the intersection of both reports plus live transport constraints.

`QualityRequest` preserves the user request. `NegotiatedProfile` records actual codec, dimensions, FPS, bitrate mode, hardware status, and any fallback reason. `SessionStats` reports unique capture/encode/decode/present FPS and stage drops.

## Media transport

Wireless media uses authenticated DTLS-SRTP with host candidates only. Wired streams use length-bounded framing with independent logical control, video, and audio channels. Frame records include protocol version, channel, flags, sequence, monotonic timestamp, and bounded payload length.

Audio timestamps share the sender monotonic clock. The receiver estimates clock offset through heartbeat exchanges and labels latency with its uncertainty. Statistics are diagnostic estimates, not glass-to-glass claims.

## Implemented framing contract

The shared core implements the following fixed 32-byte big-endian header.
This is a parser/encoder contract; authenticated transport integration is pending.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | `UXM1` magic |
| 4 | 1 | Protocol major, currently 1 |
| 5 | 1 | Protocol minor, currently 0 |
| 6 | 1 | Control=1, video=2, audio=3 |
| 7 | 1 | Header size, exactly 32 |
| 8 | 2 | Known flags only |
| 10 | 2 | Reserved, must be zero |
| 12 | 8 | Sequence |
| 20 | 8 | Sender monotonic timestamp in microseconds |
| 28 | 4 | Payload length |

Default payload ceilings: control 64 KiB, video 16 MiB, audio 256 KiB.
Decode rejects oversized lengths before allocating. Incomplete frames consume
zero bytes; successful decode reports one frame's consumed length. Major version
mismatch, unknown channel/flags, reserved fields, and non-video keyframe flags
are rejected. Debug and Release malformed-input/round-trip tests pass.

## Recovery behavior

Three missed one-second heartbeats enter `Recovering`. Reconnect uses bounded exponential backoff for up to 60 seconds and is bound to the authenticated peer and session generation. Queued stale frames are discarded and the receiver requests a new keyframe after transport restoration.
