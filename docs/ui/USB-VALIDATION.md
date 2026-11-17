# USB release validation, 13 November 2026

Status: **Wired iPhone video and sound confirmed working on the tested PC and phone.**

The package uses DouWan 4.4.0.9's unchanged native USB capture, decoder, renderer, and audio components. All 206 original native files are checked against the extraction inventory. The WPF workspace and elevated session manager are integration code, rather than a recovered source implementation of the proprietary engine. External AirPlay and scrcpy players are excluded.

A real session reached native render dimensions of 828 x 1792. An explicit still-image request returned a valid, decoded JPEG. During a 90-second observation, all 86 API samples retained the wired session. The user separately confirmed that moving video and sound worked in the PC viewer. API sample frequency is not a measurement of delivered video frame rate.

Two integration failures were corrected. Windows System event 225 identified AppleMobileDeviceProcess.exe as blocking the USB driver switch; startup now checks for that competing process before changing drivers. The standard-user .NET Framework host also received Access Denied from Process.HasExited when examining the elevated renderer. It now verifies process birth time and the exact executable path with limited query access, then verifies window ownership. A live comparison against the previous build reproduced its rejected window and confirmed that the corrected host recognized the same renderer and displayed the connected workspace.

The session helper preserves native Windows restart requests with a boot identity. It blocks another capture write until the required restart and permits only verified original-driver rollback during cleanup. It restores the phone's original driver, selectively restores its three owned profile switches, stops its owned native process, and removes its own temporary firewall rule. No Apple application is automatically terminated and no Windows signature policy is changed.

The host's 30 boundary checks and worker's 16 isolated checks cover package identity, malformed journals, cancellation, rollback, firewall ownership, PID identity, and restart persistence. Four monitor checks cover restoration progress and rejection of reboot/error/incomplete cleanup. A controlled production guard mutation is detected by the existing host harness. These boundary checks are distinct from the real-device video/audio observation.

The first 90-second test monitor wrongly rejected normal restoration progress, although the final journal verified that cleanup completed. Its original failed report is retained. The monitor was corrected to wait for a complete schema-3 journal, a stopped status, and the worker's exit; it still rejects an actual restart requirement or cleanup error.

Fresh actual-UI trials then passed all 15 normal-Stop checks and all 16 workspace-termination checks. Both independently verified the original Windows driver binding, completed profile rollback, removal of the session firewall rule, and the worker's exit. The workspace-termination trial killed only the owned standard-user workspace; the elevated worker completed restoration itself.

One user-performed physical cable disconnect/reconnect also passed all 16 checks. The observer detected loss, then three consecutive recovered wired samples while the same native engine and worker remained running. It then observed stable connection samples and verified complete normal-Stop cleanup. This establishes one reconnect cycle on this phone, rather than general reliability across devices or repeated stress cycles.

Only one iPhone hardware path, VID_05AC/PID_12A8, has been exercised here. Delivered frame rate, end-to-end latency, audio synchronization error, long-duration stability, repeated reconnect stress, a hard kill of the elevated worker, and power-loss recovery remain unmeasured. Other iPhone/iPad models and other receiver protocols have not been validated by this USB integration. The repository contains detailed evidence under docs/douwan-architecture and test. Private device identifiers and captures are excluded from this portable package.
