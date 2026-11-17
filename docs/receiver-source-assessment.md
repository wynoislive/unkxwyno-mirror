# Independent receiver source assessment

## Scope

This pass assessed whether the user-approved GPL-3 AirPlay-Windows v0.4.1
source can be used to replace the current DouWan-derived receiver while
respecting the master prompt's explicit restrictions on protected
authentication, encryption, DRM, and unlicensed components. The upstream
installer/release executable was not run. No files under `ready-products/`
were changed.

## Artifact identity

- Upstream: [moieric11/AirPlay-Windows v0.4.1](https://github.com/moieric11/AirPlay-Windows/releases/tag/v0.4.1)
- Tag commit: `f739810e35887cf572b69275a52dc6f32323d65d`
- Official x64 release archive SHA-256:
  `5f1792a96efffe84b220edfa7286fa7670806226aa4d0b4b6c7e33ddd7747bf9`
- Downloaded source archive SHA-256:
  `bed29f64659a1b4cbae90e3b5a7d517bd4fcde2970431b55c3accb1380cb7378`
- The release archive digest matched the SHA-256 published by the GitHub
  Releases API. Its 28 ZIP entries had no absolute, traversal, duplicate, or
  colon-containing paths. The executable member is `airplay-windows.exe`
  (1,196,032 bytes); the archive also carries its runtime DLLs and GPL notices.

## Constraint conflict

The upstream [`docs/FAIRPLAY.md`](https://github.com/moieric11/AirPlay-Windows/blob/v0.4.1/docs/FAIRPLAY.md)
describes the `/fp-setup` exchange as a DRM authentication handshake and says
the implementation replays four saved Apple TV responses, then uses a
PlayFair-derived table to decrypt the `rsaaeskey` used for the mirror stream.
The upstream [`THIRD_PARTY_NOTICES.md`](https://github.com/moieric11/AirPlay-Windows/blob/v0.4.1/THIRD_PARTY_NOTICES.md)
states that the underlying FairPlay response bytes were produced by Apple's
implementation and that no license was granted for those bytes.

This is incompatible with the user's explicit instruction not to bypass
protected authentication/encryption or bundle unlicensed proprietary material.
Without the FairPlay responses and decryption table, the upstream project says
iOS refuses to proceed to the media session and the video stream cannot be
decrypted. Removing those pieces would therefore produce a nonfunctional
receiver, which the prompt also prohibits. The GPL-3 approval does not grant a
license to the separately identified Apple-derived bytes.

## Alternative source cross-check

The MIT-licensed [`Airplay2OnWindows`](https://github.com/YimingZhanshen/Airplay2OnWindows)
repository was checked at commit `8e1e03d35f9e8b3254c4ffc529c2ef78b0995847`.
Its `AirPlay/Crypto/OmgHaxHex.cs` contains a hard-coded `DefaultSap` table, and
`AirPlay/Crypto/OmgHax.cs` consumes that table in `DecryptAesKey`. This is not a
verified compliant substitute for the FairPlay key path, so it was not adopted
or executed.

The upstream README supports Wi-Fi screen mirroring and reports testing over an
iPhone Personal Hotspot. Its USB path is Apple Personal Hotspot networking over
the cable, not direct USB video capture. It does not provide Android mirroring.

## Build and verification boundary

This host has .NET SDK 8.0.426 and Git, but no CMake, Visual Studio, MSBuild,
MSVC `cl`, vcpkg, or Rust toolchain. The upstream C++ release cannot be rebuilt
from source here without adding a substantial toolchain. The available
prebuilt release executable was not run or renamed.

No new `unknownxwyno-mirror.exe` was produced. Repackaging the prebuilt release
would include the FairPlay material above; stripping it would leave mirroring
nonfunctional. Neither option meets the supplied constraints, and a UI-only
binary would be a fake/incomplete receiver. No compile, application test, or
real-device mirror test was performed in this pass.

## Remaining requirement

To build a receiver that satisfies all stated constraints, the project needs
an AirPlay receiver implementation and FairPlay/stream-decryption components
that are licensed for this use and do not rely on replayed protected handshake
material, or an authorized Apple receiver SDK. Until then, the existing
DouWan-derived package and this GPL candidate cannot be presented as the
requested independent product.
