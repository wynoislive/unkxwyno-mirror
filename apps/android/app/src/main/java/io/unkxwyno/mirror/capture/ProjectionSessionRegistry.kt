package io.unkxwyno.mirror.capture

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

object ProjectionSessionRegistry {
    private val mutableState = MutableStateFlow<CaptureState>(CaptureState.Idle)
    val state: StateFlow<CaptureState> = mutableState.asStateFlow()

    internal fun update(value: CaptureState) {
        mutableState.value = value
    }
}
