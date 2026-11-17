# DouWan Installer Reverse-Engineering Report

## Scope

This report covers static analysis of the two supplied installer artifacts. Neither
binary was executed.

| Artifact | Size | SHA-256 |
|---|---:|---|
| `ready-products/DouWan-Video-Setup-En-4.4.0.9.exe` | 50,550,536 bytes | `81a43b30558e34af6cb9c3056b07e8bc93d88ecb23e68257bb246da07f552f02` |
| `ready-products/DouWan-Video-Setup-En-4.4.0.9-x64.exe` | 63,005,904 bytes | `9d012d7452a4b2d65bf5046e3e97ad747e38110036c7aa86666ea23553b98476` |

## Findings

Both artifacts are PE32 x86 executables with five sections and no CLI directory.
REA classified both as `not-managed`, so .NET metadata and CIL are unavailable.

The regular installer imports 160 functions from seven DLLs. The x64-named
installer is also PE32 x86 and imports 165 functions from the same seven DLLs.
The DLL set is:

- `ADVAPI32.dll`
- `SHELL32.dll`
- `ole32.dll`
- `COMCTL32.dll`
- `USER32.dll`
- `GDI32.dll`
- `KERNEL32.dll`

The imports include registry operations, token and file-security APIs, shell file
operations, COM initialization and object creation, window/message APIs, file and
process operations, environment handling, temporary-file creation, and file
copy/move/delete operations. These imports are static capabilities only; they do
not prove that every API is called or establish runtime behavior.

The import hashes differ:

- regular installer: `ced282d9b261d1462772017fe2f6972b`
- x64-named installer: `6e7f9a29f2c85394521a08b9f31f6275`

The regular installer contains 15 PE resources. The x64-named installer contains
21 resources. Both contain RT_ICON-style resources, RT_BITMAP-style resources,
version information, and manifest resources. The x64-named file additionally
contains two language variants for one resource entry.

Neither image declares an export directory, so no named or ordinal exports were
observed.

Whole-file entropy is approximately 8.0 bits per byte for both artifacts: `7.9989`
for the regular installer and `7.9993` for the x64-named installer. The regular
installer has a large high-entropy run beginning near file offset `0x64000` /
decimal `409600`; the x64-named installer begins its corresponding run at
decimal `413696`. Each run covers most of its payload. High entropy is
consistent with compressed or encrypted/packed installer payload data, but does
not distinguish those cases.

## Evidence

The complete REA bundles and operation results are stored in this directory:

- `DouWan-Video-Setup-En-4.4.0.9.evidence.json`
- `DouWan-Video-Setup-En-4.4.0.9-x64.evidence.json`
- `*.inspect-managed-artifact.json`
- `*.inspect-pe-imports.json`
- `*.inspect-pe-resources.json`
- `*.inspect-pe-load-config.json`
- `*.inspect-native-exports.json`
- `*.inspect-byte-entropy.json`

Key Evidence IDs:

- regular artifact inventory: `ev_b5bf688b4a034be6a908b7678632a13cb392c7aaffd6957e9c13ab60614a5e9a`
- x64-named artifact inventory: `ev_4eae494851641d7075c90b0895a461bcd1efd8c2b365a926a69d0408ecb87da4`
- regular managed inspection: `ev_f0cba01f86f9e50a0d0f5a4dafe39e8fe99046593ec32b6dcde0d69d85a2c010`
- x64-named managed inspection: `ev_1bd02415994a11cbda76a498eeee8e5db679ec4df7c887e3b5373f2e4de41c2b`
- regular imports: `ev_b3b1f6a46f7b4cacb809a358445ff5b88391939e4a01759b26519746015f754f`
- x64-named imports: `ev_a4cf9f2eaa6160abcec252c6b223dd600f1d338d0a08dc41d22ab461422aac95`

## Limitations and next steps

- The connected REA MCP server is unavailable, so this pass used REA CLI 6.8.0.
- `open_binary` and native decompilation were not available for this Windows PE
  target in the current CLI-only environment.
- The certificate inspection for the regular installer rejected malformed or
  unsupported PKCS#7 nesting; no signature-validity conclusion was made.
- Static imports and entropy do not establish installer behavior, network use,
  persistence, or the files installed.
- The next safe static step is to extract and inspect the embedded installer
  payload without executing it, then analyze any recovered PE/DLL/resource files.
- Runtime behavior should only be studied in a disposable Windows sandbox with
  explicit user authorization.
