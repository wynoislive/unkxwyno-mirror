# Feature Inventory

Status values: `DESIGN`, `IN PROGRESS`, `BLOCKED`, `VERIFIED`, and `UNSUPPORTED`. Only `VERIFIED` means tested on the stated platform and transport.

| Capability | Windows | macOS | Android | iOS/iPadOS |
| --- | --- | --- | --- | --- |
| Send screen | DESIGN | BLOCKED | DESIGN | BLOCKED |
| Receive/render | DESIGN | BLOCKED | DESIGN | BLOCKED |
| System audio | DESIGN | BLOCKED | DESIGN, OS restrictions apply | BLOCKED, OS restrictions apply |
| Microphone | DESIGN | BLOCKED | DESIGN | BLOCKED |
| Local-network discovery | VERIFIED HOST/PARSER ONLY | BLOCKED | IN PROGRESS | BLOCKED |
| Authenticated LAN stream | DESIGN | BLOCKED | DESIGN | BLOCKED |
| USB stream | DESIGN | BLOCKED | DESIGN | BLOCKED |
| Reconnection | DESIGN | BLOCKED | DESIGN | BLOCKED |
| Screenshot | DESIGN | BLOCKED | DESIGN | BLOCKED |
| Local recording | DESIGN | BLOCKED | DESIGN | BLOCKED |
| Capability/statistics panel | IN PROGRESS; actual stream metrics unavailable | BLOCKED | DESIGN | BLOCKED |
| Requested quality persistence | VERIFIED contract tests | BLOCKED | DESIGN | BLOCKED |
| Local diagnostics/logging | VERIFIED contract and navigation tests | BLOCKED | DESIGN | BLOCKED |

## Free quality controls

All users will see 480p, 720p, 1080p, 1440p, 2160p, Native, Auto, and Custom resolution choices plus 24, 30, 45, 60, 90, 120, and Auto FPS. Availability is technical, not commercial. Unsupported combinations remain visible with the limiting stage and highest valid alternative.

Verification above is limited to the stated host/contract scope. No independent
screen or audio stream is verified. Historical DouWan-backed behavior is
deliberately excluded from this table. Shared quality/recovery/framing tests pass
in both Debug and Release on Windows x64; these synthetic tests do not establish
physical device support.
