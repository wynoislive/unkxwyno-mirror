# Wired iPhone latency findings, 2026-11-15

## Status

The Windows render path now uses SIMD NV12 conversion, a latest-frame handoff,
and paints new frames without clearing the image area first. The latest physical
run rendered 832x1792 video. A live status sample showed 17.9 displayed FPS,
5.23 ms from host sample receipt to paint, and 348 decoded frames replaced
before paint. The display rate varied between 5 and 55 FPS in observed samples.
The phone later closed the trusted USB session after about a minute; the run
ended before sustained acceptance. Apple driver restoration then completed with
no reboot required.

The v3 development package adds a separate H.264 decode/conversion worker with
one queued compressed frame, a requested-rate NEED cadence, detailed device-PTS
and host-stage reporting, and native window controls without the DWM accent
border. Debug/Release native suites and desktop/static audits pass. The v3 app
has launched, but the wired capture test is waiting at Windows' administrator
approval prompt for the already installed temporary USB driver. No driver
change has occurred in this run; sustained post-change FPS and motion quality
are still unverified.

## Findings

### Source observations

- Before this change, `Decoder::feed` converted every decoded NV12 pixel to
  BGRA with scalar arithmetic on the USB capture worker. That worker also reads
  packets, feeds audio, and publishes frames.
- Before this change, `WM_PAINT` held `frame_mutex` for the complete
  `StretchDIBits` call. The capture worker needed the same mutex to publish the
  next frame, so a slow full-frame stretch could pause USB reads.
- The viewer uses the Windows H.264 Media Foundation transform and GDI. It has
  no D3D hardware renderer or hardware decode path. Before this change, its
  latest-frame slot coalesced UI notifications, but the paint-held mutex still
  coupled capture to painting.
- The capture worker previously decoded and converted frames synchronously
  before it could read the next USB packet. Decode/conversion now run on a
  dedicated worker, with at most one compressed frame waiting behind the active
  decode. Compressed frames remain ordered for H.264 reference stability.
- The v3 NEED pump initially used a reversed CVRP subtype constant. Packet
  subtype reads a little-endian wire word, so the request worker would not
  start. The constant and a CVRP/clock-reference regression test now match the
  session parser's numeric subtype.
- Before the final update, `WM_PAINT` cleared the entire viewer to black before
  drawing each frame. This could flash black between otherwise valid frames.

### Inferences

- Scalar color conversion and the paint-held mutex could add host-side delay
  and uneven frame delivery under CPU load, especially at larger resolutions.
  The conversion benchmark below measures this part directly; it does not
  measure the full decoder, USB transport, or screen latency.
- The earlier persistent USB read error and the latest trusted-session close
  are transport/session failures separate from the measured color conversion.
  Available logs do not identify whether the latest close came from the phone,
  the transport, or the app's session lifecycle.
- The preceding run's 348 frames replaced before paint and 17.9 displayed FPS
  show that the host received/decoded more frames than it presented at that
  sample. They do not distinguish decoder backlog, USB delivery cadence, or
  paint scheduling as the dominant cause. New v3 telemetry separates device
  PTS cadence, decode+conversion duration, sample-to-paint age, and replacements.

## Changes

- Added run-time AVX2 dispatch, SSE2 fallback, and a scalar fallback for NV12
  to BGRA conversion. The integer equations and output channel values match the
  prior converter exactly.
- Frames are now immutable shared snapshots. The capture thread swaps the
  newest frame under a short lock; the viewer releases that lock before the
  full GDI stretch. If painting falls behind, older frames are replaced rather
  than queued for delayed display.
- The viewer now preserves the previous image while drawing its replacement
  and clears only the black letterbox bars. This removes the full-frame black
  flash without adding a second full-frame copy or changing pixel values.
- The viewer reports displayed FPS, the latest host USB-sample-to-paint age,
  and the number of decoded frames replaced before paint. This metric excludes
  phone capture, encoding, and time spent in USB before the sample reaches the
  host parser. It is not end-to-end input latency.
- The v3 decode worker keeps a one-frame wait queue to limit host-side backlog.
  Its periodic NEED worker uses the selected 24–120 FPS value as a best-effort
  request cadence. NEED is a keep-sending signal, not an encoder FPS setting;
  only device PTS and displayed FPS report delivered rates.
- The viewer enables minimize, maximize, resize, and system-menu controls. It
  asks DWM for no accent-color border while retaining the normal title bar. The
  request is best-effort on Windows versions that do not support that attribute.
- The v3 status line reports requested cadence, device PTS FPS, displayed FPS,
  decode+conversion time, host sample-to-paint age, and frames replaced.

## Evidence ledger

These local IDs identify the observations recorded for this report. They are
not REA Evidence IDs.

| ID | Evidence and result |
| --- | --- |
| `LAT-SOURCE-01` | Source walk of `src/backend/independent/usb_mirror.cpp`: serial decode/conversion path and paint mutex before the change; immutable frame handoff and unlocked `StretchDIBits` after it. |
| `LAT-PIXELS-01` | `unkxwyno_usb_capture_tests`: fixed black/white/gray output, padded-stride random samples compared byte-for-byte with the scalar equations, malformed-surface rejection, and the recorded 1136x2448 H.264 fixture decode. |
| `LAT-PERF-01` | Release synthetic conversion benchmark on this host: 1920x1080 AVX2 2.746 ms/frame versus scalar 9.977 ms/frame (3.63x); 3840x2160 AVX2 12.162 ms/frame. Uniform synthetic input and isolated conversion only; CPU-specific and not a 4K60 end-to-end result. |
| `LAT-CHECKS-01` | Debug and Release CTest each passed 2/2. Static independence/package tests, desktop contract tests, native failure/discovery checks, and a 59-file published package audit passed. |
| `LAT-PACKAGE-01` | `out/windows-x64-usb-latency-20261115`: apphost SHA-256 `4fa79b8f007781e73a8b7f44728ffa3a412ffa07f04d1561bfd9ce8ea9585e24`; managed app SHA-256 `e30fcb8a1a2026d3d491842df975fefa50b720a6c8875e4879cf5e473f1fea0f`; native core SHA-256 `309cff5c55109ea754f4fc400d660da82827365cffc00b957c062c5457e1a8fb`. The apphost digest is stable across these builds; the managed and native DLL digests identify the changed code. |
| `LAT-RUNTIME-01` | Local desktop log, 2026-11-15 05:36:53–05:37:23 UTC: TLS 1.2 verified, USB video capture accepted, `go!`/`skew` sync received, then `libusb0` persistent USB read failed with Windows error -5. The UI briefly showed mirroring active. The viewer status is emitted only after a frame has been painted, so at least one frame reached GDI; no sustained FPS or sample-age reading was captured. |
| `LAT-RUNTIME-02` | Published build `out/windows-x64-usb-latency-20261115-v2` on the connected iPhone: UI reported 832x1792, observed displayed rates ranged from 5 to 55 FPS, and one live sample reported 17.9 FPS, 1,627 displayed frames, 348 replaced frames, and 5.2302 ms host sample-to-paint. The USB log at 06:09:30 UTC recorded `Trusted iPhone session failed: The iPhone closed its trusted USB session`; native capture exited at 06:09:36 UTC. This is not a sustained or end-to-end latency result. |
| `LAT-RECOVERY-01` | Historical state before reconnect: recovery was pending because no iPhone was present. The latest journal supersedes that state. |
| `LAT-RECOVERY-02` | Latest driver journal `cfc0d6cb15df422daee6030fa69455d4`: `State=Restored`, `Restored=true`, `RebootRequired=false`; Apple `oem14.inf` binding restored after capture. Windows again enumerated the iPhone under its Apple USB composite driver. |
| `LAT-ARTIFACT-01` | Supplied DouWan installer hashes still match `81a43b30558e34af6cb9c3056b07e8bc93d88ecb23e68257bb246da07f552f02` and `9d012d7452a4b2d65bf5046e3e97ad747e38110036c7aa86666ea23553b98476`. Neither installer was executed or modified. |
| `LAT-PACKAGE-02` | Latest package audit passed for `out/windows-x64-usb-latency-20261115-v2` (59 files). Apphost SHA-256 `4fa79b8f007781e73a8b7f44728ffa3a412ffa07f04c1561bfd9ce8ea9585e24`; desktop DLL `e30fcb8a1a2026d3d491842df975fefa50b720a6c8875e4879cf5e473f1fea0f`; native core `49de61c91ddf2bf750ce729bbf21710453c962a888c1c2749899a80ace12b980`. |

| `LAT-V3-CHECKS-01` | Fresh isolated Debug/Release CMake builds pass CTest 2/2 each, including the new CVRP subtype/NEED-clock test and Windows H.264 fixture decode. Desktop contract tests and static independence/package audits pass. |
| `LAT-V3-PACKAGE-01` | `out/windows-x64-usb-latency-20261115-v3` published and passes the 59-file package audit. Apphost SHA-256 `4fa79b8f007781e73a8b7f44728ffa3a412ffa07f04d1561bfd9ce8ea9585e24`; desktop DLL `94b9106507f1a55fda4549fd515363e5cc29771524e99a84d53918e09c4df477`; native core `8e02d8a73e2b0b7dd6178271098f99be5beb67ff7413d3f40f23afc32f4c45e9`. |
| `LAT-V3-RUNTIME-01` | Published v3 app launches and the Mirror iPhone USB action reaches its temporary-driver step. Windows `consent.exe` awaits administrator approval. Before approval the device binding remained Apple `usbccgp/oem14.inf`; no new driver change, live video, FPS result, or capture-window chrome measurement is claimed. |
| `LAT-V3-ARTIFACT-01` | Both protected `ready-products/` installer digests still match the preserved evidence; neither installer was executed or modified. |

No new REA Evidence IDs were generated in this pass. The code-level latency
analysis does not depend on the supplied installers; earlier binary Evidence
and its limitations remain in
[`FINDINGS.md`](backend-reconstruction-20261114/FINDINGS.md).

## Remaining verification

- Reproduce the trust-session close and determine why it occurs after about a
  minute; the current evidence does not establish whether the phone, transport,
  or session implementation closes it.
- Run a sustained motion-quality test. The available FPS samples vary widely,
  and the one sample-to-paint value is not a time average.
- Complete the v3 physical run after Windows administrator approval; verify
  the request pump starts at CVRP, inspect device/display FPS and replacement
  counts during motion, test minimize/maximize/restore, and confirm the visible
  border is gone.
- Hardware decode/render, full input-to-display timing, motion-quality review,
  and 4K60/soak acceptance remain unverified.

