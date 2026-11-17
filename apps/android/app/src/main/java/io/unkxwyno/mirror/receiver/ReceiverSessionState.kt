package io.unkxwyno.mirror.receiver

sealed interface ReceiverSessionState {
    data object Idle : ReceiverSessionState
    data object Discovering : ReceiverSessionState
    data class Connecting(val peerName: String) : ReceiverSessionState
    data class Streaming(val peerName: String, val width: Int, val height: Int, val fps: Double) : ReceiverSessionState
    data class Unavailable(val reason: String) : ReceiverSessionState
    data class Failed(val reason: String, val retryable: Boolean) : ReceiverSessionState
}
