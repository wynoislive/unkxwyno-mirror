import CoreMedia
import ScreenCaptureKit

public protocol MacSampleBufferConsumer: AnyObject, Sendable {
    func consume(_ sampleBuffer: CMSampleBuffer, type: SCStreamOutputType)
}
