# Independent USB capture implementation, 2026-11-14

## Acceptance status

**Sustained live screen mirroring is not yet accepted.** The latest published
preview reached trusted TLS 1.2, video-capture sync, and rendered live 832x1792
frames. Observed displayed FPS varied from 5 to 55; one sample showed 17.9 FPS,
5.23 ms host sample-to-paint, and 348 replaced frames. The iPhone closed its
trusted USB session after about a minute. The Apple driver was restored and
verified with no reboot required. Offline fixture decoding does not substitute
for a sustained live stream. The v3 build separates decode/conversion from USB
reads and caps the compressed-frame wait queue at one. Its published app is
currently waiting for administrator approval before the temporary capture
driver can be acquired, so post-change live acceptance is pending.

## Evidence

- Original regular-package recorder SHA-256:
  `6519aa84bdc4fd2f05ef4f3a04b9d2d830da99f5632490c6ab339cb8b1aae0a2`.
  Existing REA evidence and limitations remain in
  `backend-reconstruction-20261114/FINDINGS.md`.
- Bounded LLVM disassembly at VA `0x1003dc90` shows an eight-argument call to
  `0x10117490` with handle, `0x40`, `0x40`, `0x6400`, `0x6400`, null, zero,
  1000. The callee builds vendor-control IOCTL parameters. This is manual
  disassembly evidence, not a newly generated REA Evidence identifier. The
  optional-call gate's semantics are still unknown.
- The fresh REA helper opened the same digest-verified target; a subsequent
  pseudocode request lost its process before producing a record. No new
  decompilation/evidence bundle is claimed for that request.
- The public MIT QuickTime protocol reference is pinned in
  `THIRD_PARTY_NOTICES.md`. Its 0/1 PING acknowledgement differs from the older
  regular recorder's 1/1 packet. The original packet encoder remains covered as
  artifact evidence; the session handler uses the documented acknowledgement.
- The production DLL's offline replay of the pinned upstream FEED fixture
  decodes one image at 1136x2448. Observed pixel FNV-1a checksum on this Windows
  host: `8191432405303826909`. The test asserts actual decoded pixels/dimensions;
  it does not assert bit-identical codec output across Windows versions.

## Connected-phone observations

The original Apple parent binding is `usbccgp/oem14.inf`. Apple UMDF and generic
WinUSB interface paths both reject direct capture CreateFile access. Under the
already installed `libusb0/oem15.inf` trial binding, upstream libusb's adapter
returned `LIBUSB_ERROR_IO` for activation. The direct libusb-win32 API returned
zero for `0x40/0x52/0/2`, and live descriptors changed from four to five
configurations. Configuration 5 contains the discovered FF/2A/FF interface 2.
These numbers are observations, not hard-coded selection rules.

An initial independent session received three PING packets but no media. Later
sessions, including after physical reconnect and after adding the observed
startup control, claimed capture but timed out without a complete handshake.
Each completed driver trial verified the original Apple binding restored with
no reboot requested. Local trial journals and transport logs are preserved in
ignored `out/usb-independent-trial-20261114-*`. Serial-bearing journals are
local diagnostics and are not shipped.

## Runtime behavior

The Windows app's USB action owns one native session on a worker thread. Native
code owns the bulk reader and writer, protocol state, decoder and viewer.
Malformed lengths, unknown sync requests, partial writes, codec errors and an
idle handshake produce visible errors. Reads poll at 200 ms. Normal stop sends
HPA0/HPD0 and releases the interface; capture mode activated by the direct adapter
is disabled during its cleanup. This is not yet full STOP/RELS completion or
disconnect/recovery acceptance.

Display dimensions are negotiation hints. The selected FPS now controls a
best-effort periodic NEED request cadence; it does not set the iPhone encoder's
FPS. Device PTS and actual presented counters report delivered rates. Software
H.264 output and GDI rendering remain in use. Runtime AVX2/SSE2 conversion
preserves the previous BT.601 integer output, a dedicated decoder/conversion
worker keeps USB reads moving, and a one-frame compressed queue limits host
backlog. A short-lock latest-frame snapshot prevents GDI painting from blocking
capture reads. The viewer reports decode+conversion time, sample-to-paint age,
and frames replaced before paint; none is full input-to-display latency.
The capture window keeps standard minimize/maximize/resize controls while
requesting no DWM accent-color border.
Audio uses bounded PCM playback. Hardware decode/render, colorimetry, orientation,
A/V synchronization, physical latency/drop measurement, and soak testing remain
open. See `USB_LATENCY_FINDINGS.md` for the current performance data and evidence.

## Reproducibility

`scripts/build.ps1 -Configuration Release -Component All` builds the app and
core. `scripts/test.ps1 -Component All` runs independent-source/package audits,
desktop contracts, CTest protocol/media tests and simulated usbmux boundaries.
`unkxwyno_usb_probe.exe --replay tests/usb_capture/reference-feed.bin` exercises
the real DLL's decoder. `--mirror` performs the actual USB attempt.

`scripts/test-ios-usb-driver.ps1` defaults to a read-only preview. `-RunTrial`
requires administrator elevation, exact installed INF digests, one matching
phone, an unused workspace output directory and a healthy original binding.
A separate recovery watchdog restores the original binding if its owner exits.
Windows restart requirements and restoration failures remain visible in the
journal. This local test helper is not silently launched by the desktop app.

The desktop now has its own packaged `UsbDriverSession.ps1` broker, invoked only
when native capture reports status 4 (driver access required). The broker requires
Windows elevation, checks the exact installed INF hashes, resolves the complete
DriverStore packages, and permits one matching phone and one active driver lease.
Capture and decoding remain in the ordinary desktop process. Stop releases the
lease after native handles close; a separate broker watchdog handles owner exit.
No streaming duration limit is imposed. Journals stay in LocalAppData under
`UNKXWYNO Mirror/usb-driver-sessions`; unresolved recovery prevents another switch.
After reconnecting and any required restart, close/reopen the app and choose
Mirror iPhone USB to recover a pending session before starting another.

Connected-phone acceptance on 2026-11-15: the actual desktop button successfully
prepared the temporary driver, retried capture, opened USBMux and StartSession,
then surfaced the existing TLS reset. The broker restored `usbccgp/oem14.inf`
with problem code 0 and no reboot required. This establishes driver integration,
not screen/audio acceptance. Unplug and crash recovery remain unverified on hardware.

## Durable learnings

- Apple can register both UMDF and generic WinUSB GUIDs; a WINUSB service name
  alone does not mean CreateFile/direct capture access is supported.
- On this installed capture driver, direct libusb-win32 activates capture while
  libusb 1.0's compatibility adapter returns an I/O error. Diagnose the API layer
  before blaming trust/cable state.
- A configuration can appear after a vendor request; live descriptor reads and
  explicit interface/endpoint discovery are needed. Refresh cached contexts.
- A single H.264 sample can produce no immediate Media Foundation output. End
  the recorded stream and drain before declaring a decoder test failed.
- Original static implementation variants can use different PING values.
  Preserve artifact evidence separately from live interoperability claims.

## 2026-11-15: control channel and trusted-session work

The earlier reference picture/sound confirmation did not identify its transport.
In a later control with iPhone Wi-Fi and mobile data disabled, the user reported
no moving video/viewer in the original app. It is not a verified USB baseline.

The API metadata trace is preserved locally at
`out/usb-reference-metadata-20261114-b/usb-metadata.log`, SHA-256
`821d31fe53f27803d0bd3bfce35be6ab175ebe76454d41d2abea31b94461c463`.
It records selecting configuration 4 before activation, then configuration 5;
one handle carries USBMux and capture traffic. It does not prove that reference
run delivered video. No media or pairing payloads were recorded by the observer.

The independent direct adapter now selects the highest advertised valid USBMux
configuration before activation, claims the combined device handle and negotiates
USBMux versions, setup, TCP SYN/ACK and bounded read-only lockdownd requests.
On the actual connected phone, this completes through QueryType and ProductVersion.
The device emits link marker `FACEFACE`; the host emits `FEEDFACE`. An initial
overly strict equality check was corrected using the live header observation.
Both variants are covered in the focused protocol suite.

USB reads now retain asynchronous transfers across 200 ms polling timeouts.
They are canceled during cleanup, so ordinary polling does not repeatedly abort
other endpoints. Control input/output queues are bounded. A control-protocol
failure remains visible instead of silently trying a different adapter.

USBMux protocol/layout references were inspected at
[usbmuxd revision 3ded00c9985a5108cfc7591a309f9a23d57a8cba](https://github.com/libimobiledevice/usbmuxd/tree/3ded00c9985a5108cfc7591a309f9a23d57a8cba).
The new implementation is original code; upstream GPL source was not copied
into the product or linked into its runtime.

`UsbTrustedSession.cs` adds normal pairing and StartSession/TLS using an identity
generated for this application, rather than reading Apple/iTunes private keys.
Its public PairRecord fields follow the documented flow cross-checked in
[libimobiledevice revision fa0f79190142bc309307967c058f89c1b36eb6b8](https://github.com/libimobiledevice/libimobiledevice/tree/fa0f79190142bc309307967c058f89c1b36eb6b8).
The app's own saved identity is DPAPI-encrypted. TLS pins the public device key
and supplies the generated client certificate. The capture worker waits for
successful trusted-session establishment when invoked by the managed app.

Offline tests exercise fragmented control replies, creation of public PairRecord
fields, mutual TLS, rejection of a mismatched device key and encrypted local
identity storage. The rejection assertion requires a completed TLS exchange and
the precise key-pin error; an unrelated I/O failure cannot pass that case.

Subsequent authorized hardware trials reached normal pairing and successful
StartSession with this app's own identity. The user approved Trust. The actual
phone then reset the connection during TLS, before any server TLS record was
observed. This also reproduces through Apple's normal local USBMux service,
without the temporary capture driver, and after an iPhone restart. The capture
driver trials restored the original Apple binding; no reboot was requested.

The independent memory-BIO client now uses pinned OpenSSL 3.5.9 libraries from
the official MinGit archive, with app-owned pairing-root credentials and device
public-key pinning. It negotiates TLS 1.2 or newer. A diagnostic comparison with
the upstream client's lower protocol/security defaults also reset; that option
was removed rather than shipped. No Windows root store or TLS policy is changed.
The archive and extracted-library digests are in `THIRD_PARTY_NOTICES.md`.

At that stage, the live acceptance gate was still trusted-session TLS, followed
by capture synchronization and moving video/audio. Offline decoding and mock
mutual TLS did not satisfy those gates. Later connected-phone runs completed
TLS and rendered video, as recorded in the current performance update below.

That earlier comparison ran with the user-confirmed unlocked phone, Settings
open, and screen awake. The desktop then reached StartSession but reset TLS
before video. Subsequent runs progressed beyond that failure; the current
remaining issue is the trusted USB session closing after video starts.

## Current wired performance update, 2026-11-15

The published package `out/windows-x64-usb-latency-20261115-v2` passed Debug
and Release native CTest (2/2 each), desktop contract tests, and the 59-file
independent package audit. In the connected-phone run, the UI reported 832x1792
output and variable displayed rates (5-55 FPS in sampled updates). A live status
sample showed 17.9 FPS, 1,627 displayed frames, 348 replaced frames, and
5.2302 ms between receipt of the USB sample by the host parser and paint. The
value excludes phone capture/encode and USB time before host parsing; it is not
end-to-end latency. These data show low host-side age for that sample but do not
identify why overall cadence varies.

At 06:09:30 UTC the app logged `The iPhone closed its trusted USB session`; the
native capture exited at 06:09:36 UTC. This followed successful TLS 1.2 and
video-capture synchronization. The cause of the session close is unknown. The
latest driver journal is `Restored=true`, `RebootRequired=false`, and Windows
again enumerated the iPhone using the Apple driver.

The paint path no longer clears the video area to black before drawing each
frame. It retains the prior image until the next GDI stretch and clears only
letterbox bars, addressing a source-confirmed flash without changing image
pixels or adding an extra framebuffer copy. A visual motion review and sustained
session test remain open.
