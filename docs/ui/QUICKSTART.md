# unkxwyno mirror 1.2.0 — native iPhone USB

Run release/unkxwyno_mirror.exe. Keep the sibling unkxwyno-mirror-douwan folder beside it. The underscore and hyphen executable names start the same app. A portable copy must include the complete folder, or extract the complete ZIP.

This version reuses the original, unchanged DouWan 4.4.0.9 native engine and its USB recorder/renderer. The separate AirPlay-Windows and scrcpy players from 1.1.0 are not included. The old build and instructions remain preserved locally. DouWan's own license and native interface remain intact.

Moving video and sound have been confirmed on the tested iPhone, including a 90-second wired-session observation. See [USB-VALIDATION.md](USB-VALIDATION.md) for the verified scope and remaining measurements.

## Connect an iPhone

1. Connect one unlocked iPhone to the PC with a data cable. Accept **Trust This Computer** if prompted.
   Close iTunes/Apple Devices and end **AppleMobileDeviceProcess.exe** in Task Manager before starting capture. The helper stops before changing drivers if that competing process is running.
2. Choose **Start native USB mirroring**, then approve the Windows administrator prompt.
3. The helper temporarily selects the already installed USB capture driver for the connected phone. Native video and audio open in DouWan's video window.
4. The workspace lists **wired sessions only**. **Show live video** brings the native viewer forward. A service starting or a window opening does not count as a connected phone.
5. Choose **Stop USB mirroring** before unplugging. Wait for Apple USB driver restoration to complete.

The connection uses DouWan's USB capture path. Personal Hotspot and Control Center's Screen Mirroring are not part of this workflow. The original shared DLL is named libairplay.dll and contains the native USB recorder as well as other vendor capabilities. A temporary firewall rule blocks remote inbound connections to this engine during the USB session, while allowing its local status service.

## Compatibility and recovery

The verified device has Apple hardware ID VID_05AC&PID_12A8. The helper requires a matching capture driver already installed in Windows. It does not download/install a new driver package, add a certificate, or change Windows signature policy. Other iPhone/iPad hardware IDs, Android, and other casting protocols have not been validated by this USB integration and are not offered as substitute receivers.

During capture, normal Apple USB access such as photo transfer can be interrupted. On stop, the helper restores the phone's previous driver and removes its own firewall restriction. Other running receivers are not taken over or terminated.

If the phone disconnects, leave the app open and reconnect it. If restoration is pending because the phone was unplugged, reconnect it and allow cleanup to finish. The journal is retained for recovery on a later start; diagnostics report restoration failure rather than claiming success.

If Windows declines administrator access, startup stops. Keep the complete package together: the host checks file hashes before launch. For original vendor activation/account messages, use DouWan's own interface; this integration does not bypass those checks.

If diagnostics say Windows requires a restart, save your work and use **Restart** in Windows. Reconnecting the cable alone does not satisfy that requirement. Reopen the app afterward so it can complete the retained recovery journal before another session.

## Playback and captures

Video, audio, recording, rotation, and native playback controls remain in the original renderer. The workspace does not stream repeated screenshots as live video. **Save snapshot** requests one still image and saves it under Captures. Snapshot width affects saved images, not native video resolution or frame rate.

The status endpoint is fixed at http://127.0.0.1:17531. The native API can listen on wildcard addresses; its temporary program-specific firewall restriction is therefore part of this integration.

## Build and evidence

Build with src/build/build-desktop.ps1. It copies the verified original extracted payload from refrence/douwan-static-pass/x64, compiles the WPF host, and packages the USB worker. It does not execute the reference installers or alter the native engine.

See [architecture findings](../douwan-architecture/README.md) for binary identities and the successful original-engine USB experiment. Integrated release validation is recorded separately after testing. No unmeasured latency, frame rate, or long-run stability guarantee is implied by a successful connection.
