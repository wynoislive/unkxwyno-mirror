import Foundation

public enum MacCaptureState: Sendable, Equatable {
    case idle
    case requestingPermission
    case selectingSource
    case authorizedAwaitingTransport(displayID: UInt32)
    case capturing(displayID: UInt32, width: Int, height: Int)
    case stopped
    case unavailable(reason: String)
    case failed(reason: String, retryable: Bool)
}

public struct MacDisplaySource: Sendable, Equatable, Identifiable {
    public let id: UInt32
    public let width: Int
    public let height: Int
    public let name: String

    public init(id: UInt32, width: Int, height: Int, name: String) {
        self.id = id
        self.width = width
        self.height = height
        self.name = name
    }
}
