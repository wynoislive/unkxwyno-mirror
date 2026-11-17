import Foundation
#if canImport(ScreenCaptureKit)
import ScreenCaptureKit
#endif
#if canImport(ReplayKit)
import ReplayKit
#endif

public enum CaptureCapabilityProbe {
    public static func current(deployment: IOSCaptureDeployment = .unconfigured) -> IOSCaptureCapability {
        #if canImport(ScreenCaptureKit)
        if #available(iOS 27.0, *) {
            return IOSCaptureCapability(
                backend: .screenCaptureKit,
                available: deployment.screenCaptureKitTransportConfigured,
                requiresBroadcastExtension: false,
                explanation: deployment.screenCaptureKitTransportConfigured
                    ? "ScreenCaptureKit is configured. Capture still requires the system picker and permission."
                    : "ScreenCaptureKit exists in this SDK, but no authenticated sample-buffer transport is configured."
            )
        }
        #endif

        #if canImport(ReplayKit)
        return IOSCaptureCapability(
            backend: .replayKitBroadcastExtension,
            available: deployment.replayKitBroadcastExtensionConfigured,
            requiresBroadcastExtension: true,
            explanation: deployment.replayKitBroadcastExtensionConfigured
                ? "ReplayKit requires the user to start the UNKXWYNO broadcast extension from the system picker."
                : "ReplayKit exists in this SDK, but the broadcast extension and authenticated transport are not configured."
        )
        #else
        return IOSCaptureCapability(
            backend: .replayKitBroadcastExtension,
            available: false,
            requiresBroadcastExtension: true,
            explanation: "This SDK does not provide a supported screen-capture framework."
        )
        #endif
    }
}
