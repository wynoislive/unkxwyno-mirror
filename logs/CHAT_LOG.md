# Implementation Chat Log

This file is append-only. Entries contain sanitized implementation actions and evidence, not secrets, device identifiers, captured media, or private credentials.

## Sync 01 - 2026-11-13 - Independent foundation

### Requested outcome

Begin the approved clean-room implementation for Windows, macOS, Android, and iOS/iPadOS send/receive operation, with USB and local-network transports and a permanent free-product policy.

### Commands and observed results

- `git status --short --branch` -> failed because the workspace had no `.git` repository.
- `Get-FileHash -Algorithm SHA256` on the two installer targets and tested legacy executable -> identities matched the recorded hashes in `PROJECT_DISCOVERY.md`.
- `dotnet --info` -> .NET SDK 8.0.426; .NET 10.0.11 runtime and desktop runtime present; no .NET 10 SDK listed.
- `where.exe` checks -> Git and Java 17 found; CMake, Ninja, C/C++ compiler, Android SDK/ADB not found on `PATH`.
- Workspace inventory -> legacy evidence and build outputs remain present; new `apps/`, `core/`, `tests/`, `scripts/`, and `assets/` roots were absent at discovery time.
- gstack bootstrap check -> the skill instruction file exists, but its expected runtime `bin` directory is absent; work continued in documented degraded mode without claiming gstack execution.

### Decisions applied

- Do not execute either installer or repeat existing REA analysis.
- Preserve `ready-products/`, existing evidence, and the working legacy release.
- Build the independent runtime alongside legacy material and require a clean-machine/no-vendor package gate before promotion.
- Mark Apple builds blocked, not passed, until a Mac and real devices are available.

### Current result

Documentation and implementation work started. No independent platform build or mirroring session is claimed by this entry.

## Sync 02 - 2026-11-13 through 2026-11-14 - Foundation, core and Windows verification

### Objective and skills

Implement the approved independent source tree and prove the shared contracts,
Windows UI, settings, diagnostics, and packaging on the available host.
Used the previously read REA, analysis-optimization, CI-audit, and gstack routing
instructions. Existing binary evidence was reused; installers were not run.
gstack remained degraded because its runtime bin directory was missing. UI Taste
and UI UX Pro Max captured instructions informed image-first design; the Python
search helper was unavailable. Generated setup copy was inspected and corrected
to refer to the independent companion.

### Files and implementation

- Added independent root build metadata, MIT license, source-control exclusions,
  design tokens, README, decisions, issues, changelog, and platform documentation.
- Initialized a local Git repository; no commit, remote, or external publication.
- Implemented C++ capability negotiation, requested/actual distinction, bounded
  queues, heartbeat/recovery state, metrics, versioned C ABI and UXM1 framing.
- Added WinUI app, local mDNS parser/browser, local endpoint validation, atomic
  requested-quality settings, bounded diagnostic messages, diagnostics page,
  core probe, and responsive connection guide.
- Added Android MediaProjection/NSD and Apple capture/receiver library scaffolds.
  Their native builds remain unverified without Android SDK or Mac/Xcode.
- Added component build/test scripts, independence/package audits, controlled
  negative audit tests, native failure regression, and Windows publish script.

### Commands and observed results

- Installed .NET SDK 10.0.404, CMake 4.4.4, Ninja 1.13.2 and LLVM-MinGW UCRT
  23.1.4-20261020. WinGet verified portable download hashes before extraction.
- `dotnet restore apps/desktop/UnkxwynoMirror.Desktop.csproj --force-evaluate`
  stalled over IPv6. Restart with process-local IPv6 disabled completed; lockfile
  created and subsequent locked restores passed.
- Initial WinUI compilation rejected invented DNS-SD API calls; replaced them
  with the tested local mDNS implementation. Subsequent builds passed.
- `scripts/test.ps1 -Component Core` initially failed compilation, then linking.
  Fixed explicit SessionController initialization and C++ DLL export annotations.
- The first native run exposed PowerShell returning success despite Ninja/CTest
  failures. Added explicit native exit checks. Controlled configure failure 17
  now throws from both build and test scripts before downstream commands run.
- `scripts/test.ps1 -Component Core` -> PASS; Debug CTest 1/1 executable, containing
  quality, queue, recovery, metrics, wire-frame and C ABI assertions.
- `ctest --test-dir out/core/release --output-on-failure` -> PASS, 1/1.
- `scripts/build.ps1 -Configuration Release -Component All` -> PASS, no warnings.
- `dotnet run --project tests/desktop/UnkxwynoMirror.Desktop.Tests.csproj -c Debug
  -- out/windows-x64-core-integrated` -> PASS, including real packaged DLL load,
  quality choices, PTR/SRV parsing, local discovery bind/query/stop, settings
  round-trip/corruption/I/O fallback and JSONL logging.
- `scripts/test.ps1 -Component Static` -> PASS (last pre-integration scan 45 runtime
  files); controlled legacy/monetization regression and package tests pass.
- Initial publishes omitted XBF/PRI resources and crashed in Microsoft.UI.Xaml.
  Added project publish-resource target and required-resource checks. Published
  artifact retested successfully.
- `scripts/publish-windows.ps1 -OutputPath out/windows-x64-core-integrated` -> PASS;
  52-file legacy-name/hash audit PASS.
- UI Automation against that published EXE -> responsive window titled UNKXWYNO
  Mirror, status `Independent core 0.1.0` through ABI 1.
- `llvm-readobj --coff-imports` -> compiler runtime DLL dependency removed by static
  runtime linking; remaining imports are Windows system/UCRT libraries.
- Runtime PrintWindow captures inspected at 1280x832 and 960x640. Fixed clipped
  primary actions, diagnostics overlap, and setup rail overflow. Compact guide
  invocation through UI Automation passed.
- Final installer/legacy SHA-256 checks matched the preserved discovery hashes.

### Failures, preservation and limitations

No ready-products or legacy release files were modified, deleted or moved.
Only task-created test scratch directories were removed by test cleanup. Failed
publish folders and an abandoned partial CMake download remain preserved in
ignored build/cache locations. One raw whole-package string scan was too broad:
Windows SDK projection metadata includes Miracast API names, so that scan cannot
prove a runtime protocol dependency. Authored-source and package identity audits
are the scoped checks reported above.

No independent physical-device screen/audio stream, secure pairing, USB transport,
hardware encoding/rendering, clean-machine installer, soak/performance pass, or
4K120 evidence exists. The development EXE is not promoted to the legacy release.

### Next action

Complete authenticated local signaling and a synthetic sender/receiver vertical
slice, then wire capture/codec/render adapters and validate on real devices.
Keep all stream and device support claims incomplete until that evidence exists.

### Sync 02 closing verification

`scripts/test.ps1 -Component All` passed after the final source changes: static
audit (45 runtime source files), controlled audit regressions, desktop contracts,
and core CTest. `tests/scripts/NativeFailure.Tests.ps1` also passed. The source
tree and published artifact remain saved locally for the next integration sync.

## Sync 03 - 2026-11-14 - USB priority and requested live preview

- Read the named REA, CI-audit and gstack instructions. Reused saved USB evidence
  and verified original installer/release hashes. No connected REA tools were
  available; followed recovery guidance and used existing evidence instead of
  duplicating native analysis. gstack startup scripts remained missing.
- Reconfirmed the independence requirement versus legacy engine-reuse boundary;
  asked the user which USB architecture to prioritize. No answer to that choice
  arrived during this sync. The user confirmed cable connection and requested
  the EXE preview first.
- Added `apps/desktop/Services/Usb/UsbmuxClient.cs`, a bounded loopback-only
  plist client with USB enumeration and owned raw tunnels. Added `tests/usb`
  production-boundary tests and the Usb selector in `scripts/test.ps1`.
- `dotnet run --project tests/usb/UnkxwynoMirror.Usb.Tests.csproj -- --host`
  initially passed against the real service with zero devices. After cable
  connection the host lane returned nonzero on connection refusal. Windows PnP
  independently showed Apple iPhone/composite devices with OK status.
- Added actual USB checks to MainWindow's wide and compact layouts, cancellation
  on window close, bounded waiting, and actionable service errors.
- USB synthetic suite PASS. Release build PASS with zero warnings/errors.
- Published to `out/windows-x64-usb-preview-v2`; 52-file package audit PASS.
- Opened the previously tested EXE immediately for the requested preview, then
  gracefully closed only that owned preview and its superseded update before
  opening the latest USB preview. Latest window remains open for the user.
- UI Automation invoked Check iPhone USB, read service unavailable, verified
  re-enabled control and responsive window. Physical screen mirroring is unproven.
- No driver changes, installer execution, or file deletion. Reference release
  and supplied binaries remain hash-identical.
- Recorded findings and verification limits in `docs/USB_PROGRESS.md`.

## Sync 04 - 2026-11-14 - Requested USB retest

- Ran `tests/usb/UnkxwynoMirror.Usb.Tests.csproj -- --host`: protocol boundary
  suite passed; real Apple service responded with zero USB devices.
- Independently sent bounded ListDevices to loopback port 27015 and inspected
  only key names/counts: DeviceList was actually empty, not filtered by the UI.
- Windows PnP showed the iPhone, USB device/composite and Ethernet interfaces.
  USB device service was WINUSB, composite service usbccgp, INF oem14.inf; both OK.
- Invoked the open preview's visible USB check through UI Automation. It reported
  service running/no phone visible, re-enabled the button, and stayed responsive.
- No drivers or processes were changed. Device trust/service discovery remains
  unresolved; screen mirroring was not verified by this retest.

## Sync 05 - 2026-11-14 - Source reconstruction of the native backend

### Request and scope

User explicitly rejected restoring the old receiver. Analyze both wired and
wireless DouWan paths and implement independently in source. USB remains the
priority. A 100% stability guarantee is not asserted; physical media/stability
acceptance remains open.

### Skills, evidence and tools

- Applied the named REA, CI-audit and gstack instructions. Per REA's staged
  investigation instructions, split read-only saved USB and wireless evidence
  questions into distinct agent scopes. No agent executed the original target.
- Verified supplied installers and the regular USB DLL against preserved hashes.
- No connected REA MCP tools were available. Existing REA CLI 6.8.0 doctor
  reported old default Node and missing shell Ghidra configuration. Reused the
  existing Node 26.13.0 runtime rather than upgrading system Node.
- Restored the previously acquired Ghidra 12.1.4 archive after checking SHA-256
  ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db.
  Restored its existing JDK21 archive; java.exe Authenticode was Valid. Configured
  only the analysis subprocess environment; no MCP registration was rewritten.
- Reconnected through the existing static-only local REA SDK helper. Fresh
  usbscreen analysis completed. Saved ten records in the exported
  `docs/backend-reconstruction-20261114/raw/usb-initialization.evidence.json`.
- The installed REA schema rejected unsupported `detail` and `size` fields in
  two calls; retained failures, inspected actual schemas and corrected those
  calls. No unchanged failed call was retried.
- Fresh initialization evidence confirms lockdown port62078, client label
  ownership, HostID/SystemBUID StartSession and EnableSessionSSL handling.
  No private keys or real pairing-record contents were read.
- Corrected the earlier suggested 0x1003a1a0 startup label: fresh decompilation
  establishes it is a destructor. It was not implemented as capture activation.
- Wireless artifact observations contain AirPlay/RAOP discovery, RTSP, pairing,
  RTP and protected media negotiation markers. Active handshake is unresolved;
  no protected response tables or decryption material were adopted.

### Source implementation

- Added `src/backend/independent/usb_capture_protocol.hpp/.cpp`: bounded
  incremental packet reader, numeric LE32 tags, exact ping and need encoders,
  nested sbuf validation, endpoint selection and ZLP decision helper.
- Added `winusb_capture_transport.hpp/.cpp`: already-exposed FF/2A/FF interface
  selection, Apple VID validation, bulk endpoint validation, 16 KiB reads,
  read/write timeout policies, serialized writes, stop cancellation and owned
  handle cleanup. Capture mode is not activated and transport is not auto-opened.
- Compiled the independent source into the shared core DLL; original DLLs are
  not linked. Added source README and dated evidence-backed findings.
- Scoped Git exclusions to preserve legacy sources while exposing the new
  independent subtree. Extended independence audit to scan that subtree.

### Tests and results

- Added a second CTest executable with golden bytes, every ping split,4096
  coalesced packets, unknown tags, malformed/oversized lengths, explicit reset,
  nested sample boundaries, endpoint directions/types, and invalid path rejection.
- Debug CTest passed2/2. Release core and desktop build passed with no warnings.
- Controlled mutation outside the source tree changed a ping word from1 to2;
  the golden-wire assertion failed with exit1. Production source was unchanged.
- Native runner now requires both executables to be discovered exactly once and
  uses no-tests=error. Controlled missing-USB-test inventory fails before running
  tests. Added this and native-exit regression checks to the Core lane.
- `scripts/test.ps1 -Component All` passed:50 runtime-source audit files,
  controlled audit regressions, desktop contracts,2 native executables, native
  failure/discovery checks and usbmux protocol boundary tests.
- `scripts/build.ps1 -Configuration Release -Component All` passed, zero warnings.

### Preservation and remaining work

No original receiver was restored or executed. No phone driver, trust record,
device configuration or original release was changed. Temporary analysis and
mutation directories are preserved outside tracked source. Raw reverse-engineering
output is excluded from distribution.

Capture-mode activation, full synchronization/property schemas, codec sample
decoding/rendering/audio and real wired recovery remain open. Wireless pairing,
media negotiation and interoperability remain open. The current preview is not
promoted as a working screen receiver.

Durable learnings: recovered FourCC-like constants require actual little-endian
byte assertions; USB read boundaries are not packet boundaries; the client label
must not be invented as a service name; native test discovery needs its own gate.
gstack startup binaries remain absent, so degraded-mode workflow applied and
telemetry/learnings commands could not run. Findings are recorded here instead.

### Sync 05 closing analysis and verification

- Wireless Ghidra startup returned provider_timeout at330000 ms, incomplete
  output, no confirmed OOM. Error retained as raw/017-search_strings*.json.
  Bounded saved/artifact evidence remains available; this timeout does not prove
  the wireless handlers absent. No unchanged analysis retry was issued.
- Consulted primary MIT-licensed QuickTime USB protocol notes to cross-check
  observed packet families. Its documented activation request is not yet tied
  to the original binary call site and was not executed or wired into startup.
- Exported before USB target close (ten records). Post-wireless export contained
  zero records; recorded that limitation rather than calling it a complete
  combined bundle. Closed both targets; owned REA helper exited normally.
- After final endpoint-selection changes, full All tests, combined Release
  build, Release CTest2/2 and actual desktop core-ABI probe passed. No hardware
  capture or streaming pass was obtained. Existing preview/release was preserved.

## Sync 06 - 2026-11-14 - Connected iPhone USB acceptance retest

- User asked to test current functionality, prioritizing the connected iPhone.
- Live usbmux host probe responded with one physical USB device. Windows PnP
  showed Apple iPhone, USB device/composite and Ethernet interfaces with OK status.
  The normal USB interface reports class FF/subclass FE/protocol02; this bounded
  PnP check does not establish an active FF/2A/FF capture interface.
- `scripts/test.ps1 -Component All` passed all source/package audits, desktop
  contracts, native CTest2/2, controlled native failure/discovery regressions and
  simulated usbmux protocol tests.
- `scripts/build.ps1 -Configuration Release -Component All` passed with zero
  warnings and zero errors.
- Launched the freshly built Release app and invoked its visible USB check
  through Windows UI Automation. Actual output: Apple USB service detected1
  wired device; screen mirroring still awaiting capture integration. Core status:
  Independent core0.1.0, Media setup required. Window remained responsive and the
  button re-enabled. Closed only the owned acceptance-test window afterward.
- Source inspection confirms no desktop call into WinUsbCaptureTransport or
  read_packet. Native capture activation, video/audio decode and presentation are
  not connected to the UI. Actual moving video/audio acceptance therefore remains
  incomplete; the passing boundary tests are not a hardware streaming pass.
- No drivers, phone configuration or implementation source changed in this test.

## Sync 07 - 2026-11-14/15 - Independent USB capture implementation and control tests

- Replaced the desktop's USB-count action with native capture start/stop. Added
  independent CoreMedia session parsing, Windows H.264 decoding, PCM playback,
  a native viewer and measured displayed-frame counters. No proprietary receiver
  is loaded by the independent application.
- Added Apple interface discovery, WinUSB, upstream libusb and direct system
  libusb-win32 adapters. Direct libusb-win32 activation succeeds where upstream
  libusb's compatibility adapter returns LIBUSB_ERROR_IO on the installed driver.
  Live descriptors change from four to five configurations; FF/2A/FF capture
  interface 2 is discovered and claimed. Endpoint numbers are not hard-coded.
- Recorded upstream MIT video fixture decodes through the production DLL to one
  actual 1136x2448 image (host FNV checksum 8191432405303826909). CTest covers
  decoded pixels, correlation, truncated sync requests and sample bounds.
- Full All tests passed; final Core recheck passed 2/2 plus native failure and
  discovery regressions. Release All build passed with no warnings/errors.
  Published independent development artifact: out/windows-x64-usb-capture-preview,
  55 files, source/package audits passed. libusb runtime digest and LGPL notices
  are pinned/packaged. A production mirror pass is still absent.
- User-authorized temporary driver trials used only installed, digest-verified
  oem15.inf/libusb0 and restored oem14.inf/usbccgp. Each completed trial journal
  reports restored=true and rebootRequired=false. Added a separate recovery
  watchdog. No installer or ready-products artifact was executed or modified.
- Initial direct session received repeated PING; later attempts timed out without
  full handshake. An observed original recorder vendor start request was added
  from manual disassembly at VA 0x1003dc90, target digest 6519aa84...b1aae0a2.
  A bounded PING probe also failed and was removed from production startup.
- Original reference control was run separately under earlier tracing approval.
  User confirmed moving video and sound. Its connection type was not independently
  verified in that control; a subsequent USB-only comparison is pending.
- Built an analysis-only metadata observer with MinHook v1.3.4 (revision
  c3fcafdc10146beb5919319d0683e44e3c30d537). Import-table observation alone saw
  no USB traffic; API entry hooks then captured IOCTL metadata and recognized
  capture headers. No pairing records, private keys or media payloads were logged.
- Metadata trace shows the original selects ordinary configuration 4 before the
  activation request, then configuration 5. It claims interface 1 and operates
  USBMux endpoints 0x04/0x85 alongside capture 0x05/0x86 on the same handle.
  Host PING uses 1/1. These are reference observations, not proof that the traced
  run delivered video. No new REA Evidence identifier is claimed for this trace.
- After interruption, rechecked all latest journals and live PnP: Apple driver
  restored, no reference/probe process remains. Continuing with the missing
  USBMux/initialization path and a USB-only reference comparison.

## Sync 08 - 2026-11-15 - USBMux and app-owned trusted session

- USB-only reference control (phone Wi-Fi/mobile data off) did not show moving
  video. Earlier picture/sound confirmation cannot be counted as USB acceptance.
- Implemented the USB-level multiplexor, v1/v2 framing, setup, TCP control tunnel,
  bounded replies and read-only lockdownd checks. Corrected the observed device
  FACEFACE marker versus the host FEEDFACE marker. Physical USB control negotiation,
  QueryType and ProductVersion now pass; actual device reports iOS18.7.11.
- The direct adapter selects the valid initial USBMux configuration before
  activation, shares its claimed handle with capture, and keeps asynchronous
  reads pending across polling timeouts rather than canceling endpoint traffic.
- Added normal Pair/StartSession/TLS with a newly generated application identity,
  encrypted locally with user DPAPI. No Apple/iTunes pair record/private key was
  read. User confirmed the phone's Trust prompt was approved.
- Physical StartSession succeeds with the app-owned identity; the initial TLS
  path was reset by the phone. Added stage/error diagnostics without recording
  pairing payloads or certificate/key bytes.
- Offline tests exercise mutual TLS, mismatched-device rejection, fragmented
  control frames, protected identity storage and approval/reset recovery. They
  caught Schannel's ephemeral-key incompatibility, missing canonical plist
  headers and incorrect fresh-identity startup ordering; those were corrected.
- Inspection of upstream idevice.c identified another concrete mismatch:
  lockdownd uses the pairing root identity for TLS client authentication. A new
  regression rejected the host leaf. The app now presents its own generated root
  identity; the earlier app identity is preserved in its local secure directory.
  The corrected root-identity TLS tests and managed hardware build pass.
- All suites passed before the final root-identity correction; a final full
  suite and hardware acceptance are still pending. The latest hardware trial is
  out/usb-trusted-capture-20261115-h. Do not claim live screen/audio streaming.

## Sync 09 - 2026-11-15 - post-restart TLS diagnosis and reproducibility

- The user restarted and reconnected the iPhone. Its app-owned identity still
  reaches StartSession, then the connection resets before a server TLS record
  is observed. The same failure occurs through Apple's localhost USBMux service,
  so it is not isolated to the independent raw USB multiplexer.
- Added an independent OpenSSL memory-BIO client. Offline mutual TLS and exact
  device-public-key pinning pass. A temporary comparison with upstream's lower
  minimum/security defaults also failed; that diagnostic switch was removed.
  Production permits TLS 1.2 or newer, with no weakened global policy or pin.
- Paired certificates retain the observed protocol's empty names/zero serial,
  device subject-key identifier and app-owned root TLS identity. Existing app
  identities are preserved; no Apple/iTunes private record is read.
- Replaced the host-installed Git dependency with exact-member extraction from
  the official MinGit 2.56.1 archive. Both DLLs and license match individually
  pinned hashes; the complete archive matches its published SHA-256. No archive
  executable is run. Tests reject an untrusted explicit archive without changing
  the prepared dependencies.
- Tightened the wrong-device-key test: it must reach mutual TLS and fail with
  the exact pin mismatch. A generic socket failure can no longer turn it green.
- Latest checked driver journal reports restored=true, rebootRequired=false;
  present Apple USB devices are healthy. Apple's current logs separately report
  password-protected-device errors. Requested a screen-awake, unlocked comparison;
  this is a hypothesis to test, not an established TLS root cause.
- Durable learning: upstream lockdownd uses the pairing-root TLS identity.
  Keep offline TLS tests separate from live acceptance, and require negative
  tests to prove they reached the security check rather than failing earlier.
  Build portable dependencies from digest-verified distributions, not local paths.
- Final automated All suite passed; Release All build passed with zero warnings
  and zero errors. Focused package-audit tests additionally reject missing or
  partial TLS dependencies. Published 58-file preview passed its package audit
  and launched a responsive UNKXWYNO Mirror window; left open for the user.
  Preview: out/windows-x64-usb-trusted-preview-20261115. Apphost SHA-256:
  4fa79b8f007781e73a8b7f44728ffa3a412ffa07f04d1561bfd9ce8ea9585e24.
  This is a development preview; the legacy release was not overwritten and
  live USB screen/audio acceptance remains blocked pending the phone test.

## Sync 10 - 2026-11-15 - desktop USB driver integration

- User supplied a screenshot of the desktop failing at Apple-driver capture
  access. Implemented typed native driver-required status and a desktop-owned
  temporary driver lease. Only this status retries capture, once; TLS/codec
  failures do not cause driver-switch loops. Capture remains unelevated.
- Packaged an elevated driver broker with installed-INF hash checks, DriverStore
  resolution, single-owner mutex, durable journal, process-identity checks,
  crash watchdog and restoration. It uses only already installed drivers.
  Session length is unrestricted. Pending recovery blocks another driver switch.
- Live testing caught PowerShell marshaling a reserved string argument as empty
  instead of NULL. Changed the production API declaration to IntPtr and added a
  read-only Windows regression resolving an installed INF to its matching store
  copy. Also corrected detected-device/libusb access errors to reach setup when
  Apple's interface enumeration is unavailable after its client exits.
- Actual desktop UI invocation reached capture config/interface, USBMux,
  QueryType/ProductVersion and StartSession. TLS still reset before media.
  Driver journal e5ad74af6d564f8098974eafbd30dd5e reports Restored=true,
  RebootRequired=false; independently verified usbccgp/oem14.inf/problem code 0.
- Full automated lane and Release build passed. Final native changes passed both
  Release CTest executables. Session tests cover retry ownership, canceled UAC,
  canceled acquisition, TLS failures and restoration failure reporting. Final
  package audit passed (59 files); preview left open under
  out/windows-x64-usb-driver-session-20261115. Live mirroring remains unaccepted.
- Home content now scrolls to keep long errors readable at smaller window sizes.
- Durable learning: use an actual null pointer for reserved Windows API parameters
  across PowerShell/PInvoke. Resolve installed INF packages read-only before
  driver mutation. Normal-driver restoration must check actual binding before
  repeating a needless update. Mock lifecycle tests do not prove crash recovery.

## Sync 11 - 2026-11-15 - confirmed unlocked-phone retest

- User confirmed the iPhone was unlocked with Settings open and screen awake.
  Invoked Mirror iPhone USB through UI Automation in the current desktop preview.
- Actual application logs at 01:01:53-54 UTC show successful capture-interface
  activation/claim, USBMux, read-only control and accepted StartSession. The phone
  then reset TLS before video. No moving video/audio pass occurred. Keeping the
  phone awake did not resolve the failure; do not ask for the same unlock/restart
  cycle again without new evidence. The cause remains in the unresolved trusted
  session interoperability path, not established as a user setup error.

## Sync 12 - 2026-11-15 - USB render latency and latest device retest

### Context and scope

- Reviewed all prior chat-log entries first. The latest target selected for
  execution was the independent development package, not either supplied
  DouWan installer. Both installer SHA-256 values were rechecked; neither
  installer or `ready-products/` file was executed or modified.
- The gstack router startup binary is absent (`SKILL_START_PROTO: missing`), and
  no dedicated `/qa` skill is installed. Applied degraded-mode QA manually.
  REA analysis tools are not connected; this pass used source and local app logs,
  so no new REA Evidence IDs are claimed. Earlier static Evidence remains linked
  from `docs/backend-reconstruction-20261114/FINDINGS.md`.

### Latency findings and changes

- The former live path converted every H.264 decoder NV12 surface to BGRA with
  scalar per-pixel arithmetic on the USB worker. The viewer held `frame_mutex`
  throughout `StretchDIBits`, preventing capture from publishing a frame until
  the full software paint completed.
- Added run-time AVX2 conversion, SSE2 fallback and scalar fallback, preserving
  the same pixel equations. Added immutable latest-frame snapshots so GDI paint
  does not hold the producer's lock. Older decoded frames are replaced rather
  than queued behind a slow paint.
- Added host USB-sample-to-paint age and replaced-frame count to viewer status.
  These measurements omit phone capture/encode and USB time before host parsing;
  they are not end-to-end input latency. Windows H.264 decoding and GDI remain
  software paths.
- Added report `docs/USB_LATENCY_FINDINGS.md` with a local evidence ledger,
  build/package identities, measurements and limitations. Updated
  `KNOWN_ISSUES.md`, `docs/BUILD_STATUS.md` and the USB implementation status.

### Verification

- New Debug and Release CMake builds used isolated `out/core/latency-debug` and
  `out/core/latency-release` directories because copied-workspace CMake caches
  pointed at the original `D:/unkxwyno-mirror` path.
- Debug and Release CTest each passed 2/2. Conversion tests compare SIMD output
  byte-for-byte with the scalar equations for padded-stride inputs, check
  expected black/white/gray pixels and malformed surfaces, and retain the
  recorded 1136x2448 H.264 fixture decode.
- Release synthetic conversion benchmark on this host: AVX2 1920x1080 2.746
  ms/frame versus scalar 9.977 ms/frame (3.63x); AVX2 3840x2160 12.162 ms/frame.
  These isolate color conversion and are not full-pipeline or 4K60 proof.
- Source/package audit and controlled regressions passed. Desktop contract tests
  passed. The 59-file package audit passed for
  `out/windows-x64-usb-latency-20261115`.
- Package companion identities: apphost SHA-256
  `4fa79b8f007781e73a8b7f44728ffa3a412ffa07f04d1561bfd9ce8ea9585e24`, managed
  desktop DLL `e30fcb8a1a2026d3d491842df975fefa50b720a6c8875e4879cf5e473f1fea0f`,
  native core DLL `309cff5c55109ea754f4fc400d660da82827365cffc00b957c062c5457e1a8fb`.

### Latest physical run and remaining issue

- The published app launched and the USB action invoked through UI Automation.
  Local diagnostics at 2026-11-15 05:36:53 UTC record TLS 1.2 verification,
  accepted USB video capture and subsequent `go!`, `time`, and `skew` syncs.
  The UI briefly reported mirroring active; its code emits that state only after
  a frame is painted, so at least one frame reached GDI. A sustained frame-rate
  or sample-age reading was not captured.
- At 05:37:23 UTC the persistent USBMux read failed in libusb with Windows error
  -5 (“device attached ... is not functioning”). Native capture exited at
  05:37:25 UTC. This does not establish whether the cause is the cable, phone,
  device driver or USB host stack; it is separate from the render-conversion cost.
- The newest driver journal is `RecoveryRequired`, `Restored=false`,
  `RebootRequired=false`, with the verified original/capture packages recorded
  as `oem14.inf`/`oem15.inf`. A follow-up present-device query found no iPhone,
  so the same device must be reconnected before recovery can verify the Apple
  binding. The failed journal is preserved; the latest app process was closed.
- User reiterated that shaking and lag remain the main issue and acknowledged
  the reconnect request; no later present-device result has been observed.

### Durable learnings

- In these copied workspaces, existing `out/core/*/CMakeCache.txt` files can
  refer to another checkout. Use fresh build directories instead of deleting
  historical evidence/cache outputs.
- Hash the managed desktop DLL and native core DLL as well as the apphost; the
  apphost hash stayed unchanged across this application update.
- A captured `TLS 1.2`/video-sync handshake is distinct from sustained video.
  The libusb -5 read error and restoration journal are the current live blockers.
- gstack logging/telemetry could not run because its `bin` directory is absent.
  Run `./setup` or `/gstack-upgrade` to restore the gstack runtime before a later
  full gstack workflow.

### Follow-up - 2026-11-15 - flicker fix and final wired metrics

- Updated the GDI viewer so each frame replaces the previous image directly;
  only letterbox bars are cleared. This removes the source-confirmed full-client
  black clear that could flash between frames. It adds no full-frame copy and
  leaves decoded pixel values and scaling mode unchanged.
- Fresh Debug and Release CMake builds each passed CTest 2/2. Desktop USB
  session-contract tests passed. Published
  `out/windows-x64-usb-latency-20261115-v2` and passed its 59-file independent
  package audit. Package DLL identities are recorded in
  `docs/USB_LATENCY_FINDINGS.md`.
- On the connected iPhone, TLS 1.2 and the USB video handshake succeeded and the
  viewer displayed 832x1792 frames. Sampled displayed FPS varied from 5 to 55.
  One live status reading was 17.9 FPS, 1,627 displayed frames, 348 frames
  replaced before paint, and 5.2302 ms host USB-sample-to-paint. This metric is
  host-side only and is not input-to-display latency.
- At 06:09:30 UTC the app logged that the iPhone closed the trusted USB session;
  native capture exited at 06:09:36 UTC. This was distinct from the earlier
  libusb -5 read failure. The current cause is unknown, so sustained mirroring
  and motion-quality acceptance remain open.
- The latest driver journal `cfc0d6cb15df422daee6030fa69455d4` records
  `Restored=true`, `RebootRequired=false`; Apple `oem14.inf` was restored and
  the iPhone was again present under the Apple USB composite driver.
- Supplied `ready-products/` installer hashes remained unchanged; neither
  installer was executed or modified. The gstack router runtime remains absent,
  and no new REA Evidence IDs were available in this environment.

### Follow-up - 2026-11-15 - v3 request cadence and viewer chrome

- Reviewed the prior chat log and continued from the measured v2 run. The source
  showed synchronous H.264 decode and NV12 conversion on the USB packet reader.
  v3 moves these stages to a worker and holds at most one compressed frame
  behind active decoding, preserving H.264 frame order while limiting app queue
  delay. It reports device PTS FPS, displayed FPS, decode+conversion time,
  sample-to-paint age, and frames replaced.
- Corrected the v3 CVRP dispatch value to match the little-endian subtype returned
  by the packet parser; without the match, the paced NEED worker would not start.
  Added a session test for CVRP dispatch and the phone's video clock reference.
  The selected 60 FPS setting drives a best-effort NEED request cadence; it
  cannot force the iPhone encoder to produce 60 FPS.
- The capture window now explicitly has minimize, maximize, resize and system
  menu styles. It requests no Windows DWM accent-color border while keeping
  the title bar. A live visual check remains pending.
- Fresh isolated Debug and Release builds pass CTest 2/2. Desktop contract tests,
  static audits and the 59-file package audit pass. Published
  `out/windows-x64-usb-latency-20261115-v3`; apphost SHA-256
  `4fa79b8f007781e73a8b7f44728ffa3a412ffa07f04d1561bfd9ce8ea9585e24`, desktop
  DLL `94b9106507f1a55fda4549fd515363e5cc29771524e99a84d53918e09c4df477`, native
  core `8e02d8a73e2b0b7dd6178271098f99be5beb67ff7413d3f40f23afc32f4c45e9`.
- The v3 app launched and the wired USB action reached the temporary capture
  driver step. Windows `consent.exe` is awaiting administrator approval. The
  run has not switched the verified Apple driver or rendered live frames, so no
  post-change FPS or smoothness claim is made. The iPhone enumerated before the
  prompt and its original binding was `usbccgp/oem14.inf`, problem code 0.
- `ready-products/DouWan-Video-Setup-En-4.4.0.9.exe` and its x64 companion
  remained untouched. The gstack runtime remains unavailable, so manual checks
  were used; no new REA Evidence IDs were generated.
