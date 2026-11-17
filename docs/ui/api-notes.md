# Local receiver client and application settings

Implemented `src/app/MirrorApiClient.cs` and `src/app/AppSettings.cs` in namespace `UnkxwynoMirror.Desktop`. They compile with the installed .NET Framework C# compiler using `System.Net.Http.dll` and `System.Web.Extensions.dll`; no package dependency is required. The source uses C# 5-compatible syntax.

The client follows the shipped `refrence/douwan-static-pass/x64/restful/api.js` and `restful/help.html` request declarations. These are static client/documentation contracts. The tests below exercise a separate fixture server, not the proprietary receiver. Real phone operation and receiver compatibility remain unverified.

## Client surface

- `new MirrorApiClient()` defaults to `http://127.0.0.1:17531`; a local HTTP origin can be supplied explicitly.
- `ListAsync(token)` returns devices from `data.device_list`.
- `InfoAsync(uuid, token)` reads the documented cast type, image dimensions and native window handle.
- `SnapshotAsync(uuid, maxWidth, token)` requests `/api/device/snapshot2`, decodes `data.image.imageData`, and returns JPEG/PNG/BMP bytes. The packaged JavaScript uses `format`, while its help page uses `fmt`; the request sends both as `jpeg`. This compatibility choice has not been tested against the live receiver.
- `ControlAsync(uuid, action, token)` accepts the declared `back`, `home`, `power`, `notify`, `appswitch`, and `pointmode` actions. `pointmode` selects documented mode 1; it is not an automatic two-mode toggle.
- `MouseAsync(uuid, action, x, y, endX, endY, token)` uses normalized coordinates and documented button `"1"`. For `swipe`, the final two values are normalized endpoints. For `scroll`, they are `scroll_dx`/`scroll_dy` angles in the documented -360 to 360 range.
- `KeyboardAsync(uuid, keyCode, ctrl, shift, alt, token)` issues the documented `keypress` body; `metaKey` is false.
- `SetTitleAsync(uuid, title, token)` changes the documented receiver window title.

`MirrorDevice` exposes `Uuid`, `Name`, `ConnectionKind`, `CastType`, `Width`, `Height`, and `WindowHandle`. Cast types 1/2 map to iPhone USB/Wi-Fi; 3/4 map to Android USB/Wi-Fi. Devices returned by the list endpoint can omit detailed dimensions; those remain zero until `InfoAsync` is queried.

Only loopback HTTP origins are accepted. Proxy use and automatic redirects are disabled. One `HttpClient` is retained per client object, with a three-second request timeout and a 12 MiB response-buffer ceiling. Image payloads are limited to 8 MiB and must have a supported raster-image header. The class never starts a receiver or another process, does not contact remote services, and does not log frames.

Malformed response envelopes produce `InvalidDataException`. Nonzero receiver status values produce `MirrorApiException` with `ReceiverCode`; unreachable-service errors preserve an inner exception. Caller cancellation remains `OperationCanceledException`; a request timeout becomes an explanatory `TimeoutException`. The UI should display these errors inline and offer a retry rather than a blocking startup dialog.

## Settings

`AppSettings.Load()` / `Save()` use `%LocalAppData%/unkxwyno-mirror/settings.json`; they do not read or change vendor configuration. Defaults are:

| Property | Default |
|---|---|
| `ReceiverEndpoint` | `http://127.0.0.1:17531` |
| `CaptureFolder` | User Pictures folder / `unkxwyno-mirror` |
| `PreviewWidth` | 960 pixels |
| `PreviewIntervalMs` | 150 milliseconds |
| Receiver startup | Automatic whenever the desktop app opens; no opt-out setting |
| `EnginePath` | Application directory / `engine/unkxwyno-receiver.exe`; release-root entry falls back to `unkxwyno-mirror/engine/unkxwyno-receiver.exe` |

`Validate()` checks these values before a save. `LoadWarning` describes recovery from an unreadable or invalid file. Bad settings are copied to an available sibling `.invalid-...json` backup and left unchanged; defaults are returned. If the backup itself cannot be written, the existing file remains unchanged and the warning says so. Save writes a temporary file in the same directory and atomically replaces an existing file while retaining a `.previous-...json` backup. No recursive directory deletion is used. Capture files are not created by the settings class.

## Verification

Compilation passed:

```powershell
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:library /optimize+ /reference:System.dll /reference:System.Core.dll /reference:System.Net.Http.dll /reference:System.Web.Extensions.dll '/out:D:\unkxwyno-mirror\test\api-client-build\MirrorServices.dll' '.\src\app\MirrorApiClient.cs' '.\src\app\AppSettings.cs'
```

The integration fixture passed with:

```powershell
& '.\test\api-client-smoke.ps1'
```

The script compiles the two source files and an explicitly labeled test harness into the PowerShell process. A `TcpListener` binds a dynamically assigned loopback port and serves representative HTTP responses. It requires no URL reservation, firewall change, original application, real phone, or actual product settings writes.

Fifteen checks passed: device list/type mapping; device details; snapshot data URI and request fields; control/swipe/keyboard/title request fields; nonzero receiver errors; malformed JSON; missing UUID; missing response data; non-image frame rejection; refusal to follow HTTP redirects; caller cancellation; the three-second timeout; remote-origin rejection; corrupt settings preservation and save/load with backup; oversized settings recovery.

The observed result is [test fixture result](../../test/api-client-run-2da875e80b384a50a264a18aedaeccef/result.json). The fixture leaves its generated test source and settings backups under its unique `test/api-client-run-*` directory for inspection. This validates the new client and settings behavior against fixture HTTP traffic. It does not establish that the receiver API is enabled, that every documented route works in this build, or that a phone can mirror successfully.
