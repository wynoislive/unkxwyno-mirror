# Security Design

## Security boundary

UNKXWYNO accepts media and control data only from a locally paired peer. Discovery is not trust. USB attachment is not trust. Screen-capture permission is always controlled by the operating system and the user.

## Pairing and identity

- Use TLS 1.3 and per-install device identity.
- Bind first pairing to a QR-carried certificate fingerprint and one-time random token, or to an explicit same-code confirmation on both endpoints.
- Store private identity material with native secure storage; never put it in settings export or diagnostics.
- Require explicit peer revocation and reject token replay, incompatible protocol major versions, and identity changes.

## Network constraints

The runtime has no STUN, TURN, cloud relay, telemetry, advertising, payment, or automatic-update endpoint. It connects only to a discovered or user-entered peer. Firewall rules must be scoped to the application and private/local profiles.

## Input and resource safety

- Enforce frame/message size limits before allocation.
- Validate codec metadata, dimensions, FPS, timestamps, and integer arithmetic.
- Bound packet, encoded-frame, audio, decoder, and diagnostic queues.
- Fuzz protocol framing and malformed media metadata.
- Drop unauthenticated packets before expensive codec work.
- Redact tokens, keys, capture pixels, and private paths from logs.

## Protected content

The application does not circumvent DRM, secure surfaces, app audio opt-outs, operating-system capture indicators, or consent dialogs. A black/protected source is reported as an operating-system restriction.

## Supply chain

Dependencies require pinned identities, license review, checksums, SBOM entries, vulnerability checks, and redistribution notices. Legacy proprietary binaries are package-test exclusions, not fallback components.
