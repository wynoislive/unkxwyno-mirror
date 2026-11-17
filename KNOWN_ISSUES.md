# Known Issues

- Native iPhone capture renders live 832x1792 video, but frame cadence is uneven: observed samples ranged from 5 to 55 displayed FPS. One status sample showed 17.9 FPS, 5.23 ms host sample-to-paint, and 348 frames replaced before paint. The iPhone then closed the trusted USB session after about a minute; sustained video/audio is not accepted.
- The v3 path moves H.264 decoding and NV12 conversion off the USB reader, bounds the compressed-frame wait queue to one, and requests more video at the selected cadence. The `NEED` cadence is not source FPS negotiation, so 60 unique delivered FPS remains dependent on the phone and must be measured. The v3 USB run is waiting for Windows administrator approval to acquire the installed capture driver; no post-change motion result is available yet.
- The v3 capture window explicitly exposes standard minimize, maximize, resize and system-menu controls and requests that DWM remove the accent-color border. The window chrome has not yet been visually checked during a live capture session.
- The normal Apple driver rejects direct capture access on this PC. The app uses a temporary lease of the already installed capture driver and restores the Apple binding afterward. The latest journal verifies restoration with no reboot. Compatibility on other PCs, unplug-during-restoration and crash/reboot recovery still need hardware acceptance.
- The current live failure is the trusted control session closing after video starts. Its cause is unknown; the logs do not establish whether the phone, transport, or session implementation closes it. A previous run had a libusb read error -5, which also remains unexplained.
- USB FPS control, hardware decode/render, rotation metadata, colorimetry negotiation, A/V synchronization, full input-to-display latency, bandwidth and source-drop measurement, and cable-reconnection/soak acceptance remain incomplete. The viewer now reports host USB-sample-to-paint age and replaced-frame count. Unsupported sync requests fail visibly.
- Original wireless artifacts strongly indicate AirPlay/RAOP; the exact executed pairing/media workflow remains unresolved. No independent native wireless receiver is complete.

- Native hardware capture, codec, rendering, USB, and LAN streaming adapters are not yet release-validated.
- macOS and iOS projects cannot be compiled or device-tested on the current Windows host.
- Android SDK/NDK and a physical Android AOA validation matrix are not yet configured.
- The Windows independent package is not yet promoted over the tested legacy executable.
- 4K/120 FPS is a target capability, not a current claim. It stays unavailable until the exact endpoint combination passes the documented performance gate.
- Windows discovery currently uses IPv4 mDNS; IPv6-only networks and multi-interface discovery still need implementation and interoperability tests.
- Windows packages depend on the installed .NET 10 and Windows App Runtime; clean-machine deployment and installer validation are pending.
- Runtime UI captures were inspected at 1280 by 832 and 960 by 640; exact generated-target pixel matching and the full accessibility matrix remain incomplete.
- Earlier publish folders remain under ignored `out/` for diagnosis. They may contain older USB checks; use a freshly built/published artifact for current capture code.

