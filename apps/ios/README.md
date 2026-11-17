# iOS and iPadOS target

This Swift package holds platform-neutral source for the future Xcode app and broadcast-extension targets.

- iOS/iPadOS 17 through 26 use the user-started ReplayKit broadcast extension.
- iOS/iPadOS 27 and later select ScreenCaptureKit through availability checks.
- Capture fails closed when no authenticated local transport consumer is attached.
- Receiver state never reports streaming until dimensions and delivered FPS are known.

The Swift package defaults to an unconfigured deployment and therefore reports capture as unavailable. An app target must explicitly declare that its ReplayKit extension or iOS 27 ScreenCaptureKit transport is wired before the coordinator will offer capture.

The package has not been compiled on Apple hardware in this workspace. An Xcode app target, App Group, broadcast-extension target, local-network usage descriptions, signing identity, and physical-device tests are still required. That status must remain `blocked`, not `passed`, until those checks run on a Mac.
