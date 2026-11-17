import Combine
import Foundation

@MainActor
public final class IOSCaptureCoordinator: ObservableObject {
    @Published public private(set) var state: IOSCaptureState = .idle
    private let deployment: IOSCaptureDeployment
    private var authenticatedTransportAttached = false

    public init(deployment: IOSCaptureDeployment = .unconfigured) {
        self.deployment = deployment
    }

    public func requestStart() {
        let capability = CaptureCapabilityProbe.current(deployment: deployment)
        guard capability.available else {
            state = .unavailable(reason: capability.explanation)
            return
        }

        state = .awaitingSystemPicker(backend: capability.backend)
    }

    public func systemCaptureAuthorized(backend: IOSCaptureBackend) {
        let capability = CaptureCapabilityProbe.current(deployment: deployment)
        guard capability.available, capability.backend == backend else {
            state = .unavailable(reason: capability.explanation)
            return
        }
        state = .authorizedAwaitingTransport(backend: backend)
    }

    public func streamStarted(backend: IOSCaptureBackend, width: Int, height: Int) {
        let capability = CaptureCapabilityProbe.current(deployment: deployment)
        guard capability.available, capability.backend == backend else {
            state = .unavailable(reason: capability.explanation)
            return
        }
        guard authenticatedTransportAttached else {
            state = .authorizedAwaitingTransport(backend: backend)
            return
        }
        guard width > 0, height > 0 else {
            state = .failed(reason: "Capture dimensions must be positive.", retryable: false)
            return
        }
        state = .capturing(backend: backend, width: width, height: height)
    }

    public func setAuthenticatedTransportAttached(_ attached: Bool) {
        authenticatedTransportAttached = attached
        if !attached, case let .capturing(backend, _, _) = state {
            state = .authorizedAwaitingTransport(backend: backend)
        }
    }

    public func streamPaused(backend: IOSCaptureBackend) {
        state = .paused(backend: backend)
    }

    public func fail(_ reason: String, retryable: Bool) {
        state = .failed(reason: reason, retryable: retryable)
    }

    public func stop() {
        state = .idle
    }
}
