package io.unkxwyno.mirror.capture

sealed interface CaptureState {
    data object Idle : CaptureState
    data object AwaitingConsent : CaptureState
    data object AuthorizedAwaitingEncoder : CaptureState
    data class Capturing(val width: Int, val height: Int, val densityDpi: Int) : CaptureState
    data class Unavailable(val reason: String) : CaptureState
    data class Failed(val reason: String, val retryable: Boolean) : CaptureState
}
