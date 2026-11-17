package io.unkxwyno.mirror.capture

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.media.projection.MediaProjectionManager

class MediaProjectionCaptureController(context: Context) {
    private val manager = context.getSystemService(MediaProjectionManager::class.java)

    fun createConsentIntent(): Intent {
        ProjectionSessionRegistry.update(CaptureState.AwaitingConsent)
        return manager.createScreenCaptureIntent()
    }

    fun handleConsent(context: Context, resultCode: Int, data: Intent?) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            ProjectionSessionRegistry.update(CaptureState.Failed("Screen capture permission was not granted.", true))
            return
        }

        CaptureForegroundService.start(context, resultCode, data)
    }
}
