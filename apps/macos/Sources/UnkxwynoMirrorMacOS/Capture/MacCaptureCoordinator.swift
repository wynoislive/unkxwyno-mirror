import Combine
import CoreMedia
import Foundation
import ScreenCaptureKit

@MainActor
public final class MacCaptureCoordinator: NSObject, ObservableObject {
    @Published public private(set) var state: MacCaptureState = .idle
    @Published public private(set) var displays: [MacDisplaySource] = []

    private let outputQueue = DispatchQueue(label: "io.unkxwyno.mirror.capture-output", qos: .userInteractive)
    nonisolated(unsafe) private weak var consumer: MacSampleBufferConsumer?
    private var stream: SCStream?
    private var shareableDisplays: [UInt32: SCDisplay] = [:]

    public override init() {
        super.init()
    }

    public func attachConsumer(_ consumer: MacSampleBufferConsumer?) {
        self.consumer = consumer
        if consumer == nil, stream != nil {
            Task { @MainActor [weak self] in
                guard let self, self.consumer == nil else { return }
                await self.stop()
            }
        }
    }

    public func refreshSources() async {
        state = .requestingPermission
        do {
            let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: true)
            shareableDisplays = Dictionary(uniqueKeysWithValues: content.displays.map { ($0.displayID, $0) })
            displays = content.displays.map {
                MacDisplaySource(
                    id: $0.displayID,
                    width: $0.width,
                    height: $0.height,
                    name: "Display \($0.displayID)"
                )
            }.sorted { $0.id < $1.id }
            state = displays.isEmpty ? .unavailable(reason: "No shareable display is available.") : .selectingSource
        } catch {
            state = .failed(reason: error.localizedDescription, retryable: true)
        }
    }

    public func start(displayID: UInt32, preferredFramesPerSecond: Int = 60) async {
        guard stream == nil else {
            state = .failed(reason: "A capture stream is already active.", retryable: false)
            return
        }
        guard let display = shareableDisplays[displayID] else {
            state = .failed(reason: "The selected display is no longer available.", retryable: true)
            return
        }
        guard consumer != nil else {
            state = .authorizedAwaitingTransport(displayID: displayID)
            return
        }
        guard preferredFramesPerSecond > 0 else {
            state = .failed(reason: "Frame rate must be positive.", retryable: false)
            return
        }

        let configuration = SCStreamConfiguration()
        configuration.width = display.width
        configuration.height = display.height
        configuration.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(preferredFramesPerSecond))
        configuration.queueDepth = 3
        configuration.capturesAudio = true
        configuration.excludesCurrentProcessAudio = true
        configuration.showsCursor = true

        let filter = SCContentFilter(display: display, excludingWindows: [])
        let newStream = SCStream(filter: filter, configuration: configuration, delegate: self)

        do {
            try newStream.addStreamOutput(self, type: .screen, sampleHandlerQueue: outputQueue)
            try newStream.addStreamOutput(self, type: .audio, sampleHandlerQueue: outputQueue)
            try await newStream.startCapture()
            stream = newStream
            state = .capturing(displayID: displayID, width: display.width, height: display.height)
        } catch {
            state = .failed(reason: error.localizedDescription, retryable: true)
        }
    }

    public func stop() async {
        guard let active = stream else {
            state = .stopped
            return
        }
        do {
            try await active.stopCapture()
            stream = nil
            state = .stopped
        } catch {
            state = .failed(reason: error.localizedDescription, retryable: true)
        }
    }
}

extension MacCaptureCoordinator: SCStreamOutput {
    nonisolated public func stream(
        _ stream: SCStream,
        didOutputSampleBuffer sampleBuffer: CMSampleBuffer,
        of outputType: SCStreamOutputType
    ) {
        guard sampleBuffer.isValid else { return }
        consumer?.consume(sampleBuffer, type: outputType)
    }
}

extension MacCaptureCoordinator: SCStreamDelegate {
    nonisolated public func stream(_ stream: SCStream, didStopWithError error: any Error) {
        let message = error.localizedDescription
        let stoppedStream = ObjectIdentifier(stream)
        Task { @MainActor [weak self] in
            guard let self,
                  let activeStream = self.stream,
                  ObjectIdentifier(activeStream) == stoppedStream else { return }
            self.stream = nil
            self.state = .failed(reason: message, retryable: true)
        }
    }
}
