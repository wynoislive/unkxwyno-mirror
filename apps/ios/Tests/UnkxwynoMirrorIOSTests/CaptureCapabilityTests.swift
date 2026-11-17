import XCTest
@testable import UnkxwynoMirrorIOS

final class CaptureCapabilityTests: XCTestCase {
    func testInitialCoordinatorStateIsIdle() async {
        await MainActor.run {
            let coordinator = IOSCaptureCoordinator()
            XCTAssertEqual(coordinator.state, .idle)
        }
    }

    func testUnconfiguredPackageFailsClosed() async {
        await MainActor.run {
            let coordinator = IOSCaptureCoordinator()
            coordinator.requestStart()
            guard case .unavailable = coordinator.state else {
                return XCTFail("An unconfigured package must not claim capture is available.")
            }
        }
    }

    func testReceiverRejectsImpossibleStreamingMetrics() async {
        await MainActor.run {
            let receiver = IOSReceiverSessionModel()
            receiver.transition(to: .streaming(peerName: "peer", width: 0, height: 1080, framesPerSecond: 60))
            guard case .failed(_, let retryable) = receiver.state else {
                return XCTFail("Impossible metrics must not enter the streaming state.")
            }
            XCTAssertFalse(retryable)
        }
    }
}
