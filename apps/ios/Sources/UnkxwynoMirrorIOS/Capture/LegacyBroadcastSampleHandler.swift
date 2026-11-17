#if canImport(ReplayKit)
import CoreMedia
import Foundation
import ReplayKit

public protocol SampleBufferConsumer: AnyObject {
    func begin() throws
    func consume(_ sampleBuffer: CMSampleBuffer, type: RPSampleBufferType)
    func pause()
    func resume()
    func finish()
}

public final class SampleBufferSinkRegistry: @unchecked Sendable {
    public static let shared = SampleBufferSinkRegistry()
    private let lock = NSLock()
    private weak var storedConsumer: SampleBufferConsumer?

    public var consumer: SampleBufferConsumer? {
        get {
            lock.lock()
            defer { lock.unlock() }
            return storedConsumer
        }
        set {
            lock.lock()
            storedConsumer = newValue
            lock.unlock()
        }
    }

    private init() {}
}

@available(iOS, introduced: 17.0, obsoleted: 27.0)
public final class LegacyBroadcastSampleHandler: RPBroadcastSampleHandler {
    private var consumer: SampleBufferConsumer? { SampleBufferSinkRegistry.shared.consumer }
    private var active = false

    public override func broadcastStarted(withSetupInfo setupInfo: [String : NSObject]?) {
        active = false
        guard let consumer else {
            finishBroadcastWithError(
                NSError(
                    domain: "io.unkxwyno.mirror.capture",
                    code: 1,
                    userInfo: [NSLocalizedDescriptionKey: "No authenticated local transport is attached."]
                )
            )
            return
        }

        do {
            try consumer.begin()
            active = true
        } catch {
            finishBroadcastWithError(error)
        }
    }

    public override func processSampleBuffer(_ sampleBuffer: CMSampleBuffer, with sampleBufferType: RPSampleBufferType) {
        guard active else { return }
        consumer?.consume(sampleBuffer, type: sampleBufferType)
    }

    public override func broadcastPaused() {
        guard active else { return }
        consumer?.pause()
    }

    public override func broadcastResumed() {
        guard active else { return }
        consumer?.resume()
    }

    public override func broadcastFinished() {
        if active {
            consumer?.finish()
        }
        active = false
    }
}
#endif
