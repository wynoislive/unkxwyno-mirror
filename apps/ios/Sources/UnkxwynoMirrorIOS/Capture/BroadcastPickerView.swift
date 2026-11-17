#if canImport(ReplayKit) && canImport(SwiftUI) && canImport(UIKit)
import ReplayKit
import SwiftUI
import UIKit

@available(iOS, introduced: 17.0, obsoleted: 27.0)
public struct BroadcastPickerView: UIViewRepresentable {
    private let preferredExtension: String?

    public init(preferredExtension: String? = nil) {
        self.preferredExtension = preferredExtension
    }

    public func makeUIView(context: Context) -> RPSystemBroadcastPickerView {
        let picker = RPSystemBroadcastPickerView(frame: .zero)
        picker.preferredExtension = preferredExtension
        picker.showsMicrophoneButton = true
        return picker
    }

    public func updateUIView(_ uiView: RPSystemBroadcastPickerView, context: Context) {
        uiView.preferredExtension = preferredExtension
    }
}
#endif
