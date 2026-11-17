# Test Plan

## Correctness

- Unit/property tests: capability intersection, quality fallback, protocol versioning, state transitions, timestamps, queue limits, and settings validation.
- Fuzz tests: framing, lengths, protobuf limits, malformed certificates, replayed tokens, and invalid media metadata.
- Integration tests: synthetic sender/receiver, process restart, source resize, orientation, codec renegotiation, and audio-only/video-only operation.
- Platform tests: capture consent denied/cancelled, protected content, device lock, background/foreground transitions, display removal, sleep/wake, and hardware-codec failure.

## Interoperability

Exercise every sender-to-receiver direction over LAN. Exercise both stream directions for Android-desktop and iOS-desktop wired links. Record exact OS, hardware, source, codec, transport, requested profile, and actual profile.

## Stability gates

- Eight hours at 1080p60 on reference hardware with no crash or unexplained disconnect.
- Post-warm-up memory growth below 10% or 100 MiB, whichever is larger; handles, threads, and queues remain bounded.
- 100 controlled network interruptions and 100 supported cable detach/reconnect cycles recover when the device returns within 60 seconds.
- Target recovery within five seconds after rediscovery.

## Performance gates

For a declared supported 1080p60 profile: at least 95% unique-frame delivery, under 1% steady-state drops, A/V sync within +/-50 ms, and p95 estimated capture-to-present latency no greater than 120 ms wired or 180 ms LAN on the named reference hardware.

4K120 is advertised only after a 30-minute run presents at least 114 unique FPS with hardware encode/decode, a 120 Hz display, bounded queues, and visible measured statistics. Otherwise it remains unavailable with the measured limiting stage.

## Product-policy and release tests

- Scan packages and network behavior for vendor binaries, activation code, ads, payment/account SDKs, telemetry, Internet endpoints, watermarks, embedded secrets, and updater services.
- Run in a clean environment with legacy folders absent.
- Verify every quality choice is free and that no session-duration or artificial performance restriction exists.
- Verify diagnostics redact pairing tokens, private keys, captures, and local paths.
- Run keyboard, screen-reader, high-contrast, text-scaling, reduced-motion, and localization-boundary checks.

At this baseline no independent test above has passed. Test reports must identify the exact revision and environment they executed.
