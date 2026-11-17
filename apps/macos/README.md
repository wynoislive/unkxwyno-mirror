# macOS target

This Swift package contains the native macOS capture and receiver state layer for the future SwiftUI application target.

`MacCaptureCoordinator` uses ScreenCaptureKit, requests the system-authorized shareable-content list, keeps capture stopped until a real encoded-media consumer is attached, and exposes video/audio sample buffers without writing them to disk.

This package is unverified because no Mac or Xcode installation is available in the current workspace. It must remain blocked in release status until it builds and passes capture, audio, permission, reconnect, and long-session tests on physical Apple hardware.
