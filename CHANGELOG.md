# Changelog

## 2026-11-14 - Independent iPhone USB capture implementation (incomplete acceptance)

- Replaced the USB-count action with native capture start/stop and a real viewer.
- Added direct libusb-win32 and upstream libusb adapters, live USB descriptor
  discovery, hidden capture-mode activation and the observed recorder startup control.
- Added bounded session/property/sample parsing, H.264 decoding, PCM playback,
  software rendering and measured displayed-frame counters.
- Verified recorded USB media decodes to an actual 1136x2448 image through the
  production DLL. Verified physical hidden-interface activation/claim under the
  already installed capture driver. Live handshake/media acceptance still fails.
- Added a guarded, explicit administrator trial with original-driver restoration.
- No proprietary receiver component was restored. This is not a production
  mirroring release, and no 4K120 or stability pass is claimed.

## 2026-11-14 - Independent foundation verification

- Compiled the C++20 core and passed Debug/Release unit tests.
- Added bounded UXM1 control/video/audio framing and malformed-input tests.
- Fixed core DLL exports, C ABI initialization, runtime dependencies, and UI packaging path.
- Added atomic requested-quality persistence and local diagnostics.
- Built and smoke-tested the independent WinUI Release package with actual core loading.
- Fixed missing publish resources, compact layout clipping, and native-tool exit handling.
- Added a reproducible publish script and source/package regression audits.

Screen/audio streaming, secure pairing, wired adapters, and production release gates remain incomplete.

All notable independent-platform changes are recorded here. Legacy wrapper
history remains in its existing evidence documents.

## [Unreleased]

### Added

- Independent repository layout and MIT product policy.
- Versioned capability, quality-negotiation, session, and statistics contracts.
- Native platform project foundations for Windows, Android, iOS, and macOS.
- Evidence-backed build, compatibility, protocol, security, privacy, and test documentation.

### Security

- Proprietary installers, extracted runtimes, signing material, and private captures are excluded from independent source history and packaging.

