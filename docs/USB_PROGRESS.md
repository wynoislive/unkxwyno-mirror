# USB integration evidence - 2026-11-14

Priority: iPhone/iPad USB. Production mirroring remains incomplete.

## Recorded architecture boundary

The legacy native path was previously validated with video/audio and one cable
reconnect, as documented in `docs/ui/USB-VALIDATION.md`. That path reuses the
original engine and changes the phone's driver temporarily. The later master
directive requires an independent runtime. The requested choice between restoring
that legacy path first and continuing the independent path is still pending.
No legacy components or driver changes were introduced in this sync.

Reused REA Evidence for the original recorder's packet reader:
`ev_2912d020b1521f55e7356dea3460adf6b1ff89860d109b0f5aa46490b4a373b6`;
serialized writer:
`ev_6a72b8d01516c386cbbfac5cdba5ff3c45376a2b6dbe84d933657d10df74fe0d`.
These describe a dedicated recorder path, not proof that usbmux is its capture
protocol. The new usbmux client is an independent companion tunnel implementation.

## Implemented and tested

- Loopback-only Windows usbmux service client on port 27015.
- Bounded 1 MiB plist replies, exact header/tag validation, safe XML parsing,
  operation deadline/cancellation and owned-socket cleanup.
- USB-only device enumeration; network-paired devices are excluded.
- Connect request with network-order device port and a bidirectional byte tunnel.
- No trust records, private keys, serial numbers or device media read/logged.
- USB check wired into both compact and wide Windows layouts.
- Fragmented reply, network exclusion, port byte order, raw tunnel preservation,
  refusal, oversized-header, mismatched-tag and stalled-service tests pass.
- `scripts/test.ps1 -Component Usb` runs the boundary suite and fails on errors.
- Release build passes with zero warnings. Published package identity audit
  passes for 52 files.

Protocol references consulted (independent implementation, no library source copied):
[libusbmuxd header](https://github.com/libimobiledevice/libusbmuxd/blob/master/include/usbmuxd-proto.h),
[message construction](https://github.com/libimobiledevice/libusbmuxd/blob/master/src/libusbmuxd.c).
usbmux is a device communication protocol; it is not a screen-capture API.

## Live observations

Before cable connection, the iTunes AppleMobileDeviceProcess listener responded
with zero USB devices. After the user connected the phone, Windows enumerated
Apple iPhone and Apple Mobile Device USB Composite Device with OK status, but
the local Apple service listener was absent and the host probe failed with a
connection refusal. A physical USB tunnel has therefore not been verified.

The published preview is `out/windows-x64-usb-preview-v2/UnkxwynoMirror.Desktop.exe`.
It was launched for the user's preview and left open. Windows UI Automation
invoked the visible USB check and read its service-unavailable message; the
button was re-enabled and the app remained responsive. Opening Apple Devices or
iTunes and repeating the check is the next hardware observation.

## Unresolved

Physical tunnel, companion authentication, capture/media integration, audio,
driver-independent real-device recovery, and streaming performance remain open.
Synthetic service tests are not a hardware-mirroring pass.

### User-requested retest

The Apple service is now running and answers ListDevices, but its actual reply
contains an empty DeviceList. A second bounded protocol probe confirmed this
before application filtering. Windows still reports the connected iPhone and
USB interfaces with OK status (WINUSB/usbccgp, oem14.inf). The preview's USB check
shows the same no-device result and remains responsive. Phone trust state and
the reason for Apple's empty list remain unknown; no driver changes were made.

gstack status: DONE_WITH_CONCERNS for preview/QA workflow; startup scripts are
missing, so degraded-mode rules applied and telemetry was skipped. Durable
learning: Windows can enumerate an iPhone while the local usbmux service is
absent; device detection and service availability require separate checks.
