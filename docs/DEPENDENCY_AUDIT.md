# Dependency Audit

## Policy

Authored UNKXWYNO source is MIT licensed. Every third-party component requires a pinned version or commit, source URL, checksum, license, redistribution assessment, and security review before entering a release. Build downloads do not imply permission to redistribute binaries.

## Proposed independent dependencies

| Component | Purpose | License | Baseline status |
| --- | --- | --- | --- |
| CMake/C++20 toolchain | Shared core build | Tool-specific | Not installed/verified |
| Protocol Buffers | Control schema | BSD-3-Clause | Proposed, not adopted |
| libopus | Audio codec | BSD-3-Clause | Proposed, not adopted |
| libdatachannel | Host-only WebRTC | MPL-2.0 | Proposed, compliance review required |
| mbed TLS or native TLS adapters | Wired/control TLS | Apache-2.0 or OS terms | Selection not yet pinned |
| libusbmuxd | iOS wired forwarding | LGPL-2.1-or-later | Proposed, compliance and device tests required |
| Windows App SDK | Windows UI | Microsoft terms | Proposed, version not pinned |
| Android Gradle/Compose libraries | Android UI/runtime | Apache-2.0 and component terms | SDK not configured |

## Explicit exclusions

- DouWan executables, DLLs, activation/licensing components, and updater code.
- AirPlay, Cast, or Miracast receiver implementations.
- Advertising, analytics, payment, subscription, account, or monetization SDKs.
- Proprietary binaries copied from reverse-engineering targets.

## Legacy boundary

Files under `ready-products/`, extracted references, legacy object directories, and the existing tested release are not covered by the new MIT license and must not enter independent packages.

## Skill/tool status

REA skills are installed and existing evidence is reusable. The gstack instruction file is present but its runtime `bin` directory was absent at this sync, so no gstack execution or telemetry is claimed. UI skill instruction captures exist under `docs/ui/`; their source, full content, license, and revision must be verified before installation or use.
