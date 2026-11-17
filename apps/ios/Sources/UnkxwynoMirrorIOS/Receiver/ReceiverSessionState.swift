import Combine
import Foundation

public enum IOSReceiverSessionState: Sendable, Equatable {
    case idle
    case discovering
    case pairing(peerName: String)
    case negotiating(peerName: String)
    case streaming(peerName: String, width: Int, height: Int, framesPerSecond: Double)
    case recovering(peerName: String, secondsRemaining: Int)
    case unavailable(reason: String)
    case failed(reason: String, retryable: Bool)
}

@MainActor
public final class IOSReceiverSessionModel: ObservableObject {
    @Published public private(set) var state: IOSReceiverSessionState = .idle

    public init() {}

    public func transition(to newState: IOSReceiverSessionState) {
        if case let .streaming(_, width, height, framesPerSecond) = newState,
           (width <= 0 || height <= 0 || !framesPerSecond.isFinite || framesPerSecond <= 0) {
            state = .failed(reason: "Measured stream dimensions and FPS must be positive.", retryable: false)
            return
        }
        state = newState
    }
}
