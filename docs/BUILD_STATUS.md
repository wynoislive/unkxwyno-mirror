# Build Status

Snapshot: 2026-11-15, USB render-path latency work and connected-phone retest.

| Component | Status | Evidence/limitation |
| --- | --- | --- |
| Legacy Windows wrapper | VERIFIED LEGACY ONLY | Existing artifact SHA-256 `8a827624296b38ab0f025ee539e71b7d8bee75e69f9c0e412fb83ca1b2246988`; contains proprietary dependency path |
| Shared independent core | VERIFIED UNIT LANE | Fresh isolated v3 Debug and Release CTest pass 2/2 each, including CVRP request-pump dispatch and Windows decoder fixture. Earlier synthetic AVX2 conversion: 2.746 ms at 1080p and 12.162 ms at 4K; not full-pipeline timing. |
| Recovered native USB backend | PARTIAL LIVE VALIDATION | TLS 1.2 and video sync succeeded; the published build rendered 832x1792 frames. One live sample measured 17.9 displayed FPS and 5.23 ms host sample-to-paint, with 348 frames replaced before paint. Rates varied from 5 to 55 FPS. The phone closed its trusted USB session after about a minute. Driver restoration completed with no reboot. |
| Windows independent app | VERIFIED PUBLISH/LAUNCH; USB PENDING | v3 package passes the 59-file audit and its main window launches. USB action is waiting for Windows administrator approval for the temporary capture driver. The previous v2 live measurements remain the only post-flicker device result; v3 FPS and motion quality are not yet verified. |
| Windows settings/diagnostics | VERIFIED CONTRACT/SMOKE | Atomic requested-quality persistence, malformed-settings fallback, JSONL logs, diagnostics navigation, compact connection dialog tested |
| Windows local discovery | VERIFIED HOST/PARSER ONLY | Local multicast bind/query, bounded initial enumeration and stop pass; synthetic PTR/SRV parser checks pass; physical companion interoperability remains untested |
| Windows development package | VERIFIED HOST ONLY | `out/windows-x64-usb-latency-20261115-v3`; 59-file legacy-name/hash audit passes and contains the v3 native core; requires installed .NET/Windows App Runtime |
| Android app | NOT BUILT | Android SDK/ADB not configured |
| macOS app | BLOCKED | No Mac/Xcode available |
| iOS/iPadOS app | BLOCKED | No Mac/Xcode or Apple signing resources available |
| LAN interoperability | NOT TESTED | No independent endpoints built |
| Android USB | NOT TESTED | Open Accessory implementation and device tests pending |
| iOS USB | FAILS LIVE ACCEPTANCE | Trusted TLS, video-capture sync, and rendered frames verified. Display cadence varied; the phone closed the trusted USB session after about a minute. Stable long-run video/audio and end-to-end latency remain unverified. Apple driver restoration was verified. |
| 4K120 | NOT VERIFIED | Must pass the dedicated capability gate before advertising support |

Latest development preview:
`out/windows-x64-usb-latency-20261115-v3/UnkxwynoMirror.Desktop.exe`.
Fresh Debug and Release builds each passed CTest 2/2; desktop contract tests,
static audits, and the 59-file package audit passed. The app is waiting for
Windows administrator approval to acquire the capture driver; no v3 live stream
or FPS claim has been captured. The current device binding is the restored
Apple driver (`usbccgp/oem14.inf`, problem code 0). See
`USB_LATENCY_FINDINGS.md` for package hashes, performance observations, and
measurement limits.

## Host baseline

Observed: .NET SDK 8.0.426; .NET 10 runtime/desktop runtime 10.0.11; Java 17; Git present. CMake, Ninja, compiler, Android SDK/ADB, and Xcode were not found/configured.

The baseline above is historical. Installed during implementation: .NET SDK
10.0.404, CMake 4.4.4, Ninja 1.13.2, LLVM-MinGW UCRT 23.1.4-20261020.
Ninja and LLVM-MinGW portable archive hashes were verified by WinGet. The core
DLL statically links the compiler C++ runtime; import inspection shows Windows
system/UCRT DLLs rather than `libc++.dll`.

## Resolved failures

- NuGet restore stalled over IPv6; process-local `DOTNET_SYSTEM_NET_DISABLEIPV6=1`
  completed restore and generated `apps/desktop/packages.lock.json`.
- Invalid Windows DNS-SD API calls were replaced by a bounded local mDNS
  browser/parser. This remains IPv4 discovery; IPv6-only discovery is incomplete.
- Unpackaged WinUI publish omitted XBF/PRI resources and crashed before window
  creation. The project publish target now copies these resources; published
  launch and actual core load were retested successfully.
- Native explicit-constructor initialization and missing C++ DLL exports were
  fixed after real compiler/linker failures.
- Native tool failures previously continued through PowerShell and returned a
  misleading exit code. Build/test scripts now check each native exit code.
- Compact UI clipped primary actions; responsive rail collapse and a working
  connection-guide dialog were verified at 960 by 640 pixels.

## Remaining gates

No sustained live screen/audio stream, full input-to-display measurement,
hardware codec pipeline, clean-machine installer, performance/soak test, or
cross-platform interoperability pass exists yet. The latest TLS success does not
prove stable streaming. Do not promote the development artifact over the
preserved legacy release. 4K120 remains unverified.

The absence of a tool from `PATH` is a host observation, not proof that no copy exists elsewhere. No build lane is green until its current source and required reports execute successfully.
