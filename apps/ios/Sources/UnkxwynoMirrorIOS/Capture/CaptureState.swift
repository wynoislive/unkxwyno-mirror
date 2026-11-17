import Foundation

public enum IOSCaptureBackend: String, Sendable, Equatable {
    case replayKitBroadcastExtension
    case screenCaptureKit
}

public enum IOSCaptureState: Sendable, Equatable {
    case idle
    case awaitingSystemPicker(backend: IOSCaptureBackend)
    case authorizedAwaitingTransport(backend: IOSCaptureBackend)
    case capturing(backend: IOSCaptureBackend, width: Int, height: Int)
    case paused(backend: IOSCaptureBackend)
    case unavailable(reason: String)
    case failed(reason: String, retryable: Bool)
}

public struct IOSCaptureDeployment: Sendable, Equatable {
    public let replayKitBroadcastExtensionConfigured: Bool
    public let screenCaptureKitTransportConfigured: Bool

    public init(
        replayKitBroadcastExtensionConfigured: Bool = false,
        screenCaptureKitTransportConfigured: Bool = false
    ) {
        self.replayKitBroadcastExtensionConfigured = replayKitBroadcastExtensionConfigured
        self.screenCaptureKitTransportConfigured = screenCaptureKitTransportConfigured
    }

    public static let unconfigured = IOSCaptureDeployment()
}

public struct IOSCaptureCapability: Sendable, Equatable {
    public let backend: IOSCaptureBackend
    public let available: Bool
    public let requiresBroadcastExtension: Bool
    public let explanation: String

    public init(
        backend: IOSCaptureBackend,
        available: Bool,
        requiresBroadcastExtension: Bool,
        explanation: String
    ) {
        self.backend = backend
        self.available = available
        self.requiresBroadcastExtension = requiresBroadcastExtension
        self.explanation = explanation
    }
}
