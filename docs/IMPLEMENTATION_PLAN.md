# Implementation Plan

## Gate 1: evidence-safe foundation

- Preserve hashes and legacy evidence.
- Establish the independent directory tree, local source-control exclusions, MIT license, and build-status documents.
- Pin toolchains and third-party sources before dependency acquisition.

## Gate 2: shared executable contracts

- Implement capability, quality, session, error, and statistics types.
- Implement deterministic negotiation, bounded queues, recovery state, and a versioned C ABI.
- Validate with synthetic video/audio sources and parser/state tests.

## Gate 3: local-network vertical slice

- Add DNS-SD discovery, explicit pairing, local TLS signaling, and host-only media transport.
- Prove a synthetic sender and receiver without any legacy files present.
- Add Windows and Android native capture/encode/decode/render adapters, then extend the same contracts to Apple projects.

## Gate 4: wired transports

- Implement Android Open Accessory with consumer permissions and no ADB requirement.
- Implement iOS usbmux forwarding without replacing the Apple device driver.
- Exercise detach, lock, permission denial, and reconnection on physical devices.

## Gate 5: product controls

- Complete quality, audio, display, recording, diagnostic, accessibility, appearance, and settings behavior.
- Hide controls that have no implementation; show disabled capability choices only with a real reason and alternative.

## Gate 6: release

- Pass interoperability, security, free-policy, clean-machine, soak, recovery, and packaging gates.
- Release each platform only after its own hardware evidence passes.
- Archive the legacy Windows entry point by hash before promoting an independent executable.

Every sync updates `BUILD_STATUS.md`, `FEATURE_INVENTORY.md`, `KNOWN_ISSUES.md`, and the append-only chat log. A blocked prerequisite stays blocked rather than becoming a skipped green check.

## Progress as of 2026-11-14

Gate 1 foundation is implemented. Gate 2 shared contracts compile and pass Debug
and Release unit tests; synthetic media integration remains pending. Gate 3 has
Windows local discovery/parser and core-binding evidence but still requires
authenticated pairing, secure signaling, media transport and a complete stream.
Gate 5 requested-quality persistence and local diagnostics are implemented on
Windows. Gate 6 has a tested host development package only; installer,
clean-machine, hardware and stability acceptance remain open.
