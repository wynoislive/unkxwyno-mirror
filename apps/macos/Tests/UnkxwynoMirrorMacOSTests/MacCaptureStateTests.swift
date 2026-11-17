import XCTest
@testable import UnkxwynoMirrorMacOS

final class MacCaptureStateTests: XCTestCase {
    func testCoordinatorStartsIdle() async {
        await MainActor.run {
            let coordinator = MacCaptureCoordinator()
            XCTAssertEqual(coordinator.state, .idle)
        }
    }

    func testUnknownDisplayFailsWithoutStartingCapture() async {
        let coordinator = await MainActor.run { MacCaptureCoordinator() }
        await coordinator.start(displayID: UInt32.max)
        let state = await MainActor.run { coordinator.state }
        guard case .failed(_, let retryable) = state else {
            return XCTFail("An unknown display must fail before capture starts.")
        }
        XCTAssertTrue(retryable)
    }

    func testReceiverRejectsImpossibleStreamingMetrics() async {
        await MainActor.run {
            let receiver = MacReceiverSessionModel()
            receiver.transition(to: .streaming(peerName: "peer", width: 1920, height: 1080, framesPerSecond: .nan))
            guard case .failed(_, let retryable) = receiver.state else {
                return XCTFail("Impossible metrics must not enter the streaming state.")
            }
            XCTAssertFalse(retryable)
        }
    }
}
