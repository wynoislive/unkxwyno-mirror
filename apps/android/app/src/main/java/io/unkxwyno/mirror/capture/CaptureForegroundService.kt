package io.unkxwyno.mirror.capture

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.view.Surface
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

class CaptureForegroundService : Service() {
    private val binder = LocalBinder()
    private var projection: MediaProjection? = null
    private var display: VirtualDisplay? = null

    private val callback = object : MediaProjection.Callback() {
        override fun onStop() {
            releaseProjection(stopProjection = false)
            ProjectionSessionRegistry.update(CaptureState.Idle)
            stopSelf()
        }
    }

    inner class LocalBinder : Binder() {
        fun attachEncoderSurface(surface: Surface, width: Int, height: Int, densityDpi: Int): Result<Unit> {
            if (width <= 0 || height <= 0 || densityDpi <= 0) {
                return Result.failure(IllegalArgumentException("Capture dimensions and density must be positive."))
            }
            val activeProjection = projection
                ?: return Result.failure(IllegalStateException("No authorized MediaProjection session is active."))

            return runCatching {
                val activeDisplay = display
                if (activeDisplay == null) {
                    display = activeProjection.createVirtualDisplay(
                        "UNKXWYNO Mirror",
                        width,
                        height,
                        densityDpi,
                        DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                        surface,
                        null,
                        null,
                    )
                } else {
                    // Android 14+ permits only one createVirtualDisplay call per consent token.
                    activeDisplay.resize(width, height, densityDpi)
                    activeDisplay.surface = surface
                }
                ProjectionSessionRegistry.update(CaptureState.Capturing(width, height, densityDpi))
            }.onFailure {
                ProjectionSessionRegistry.update(
                    CaptureState.Failed(it.message ?: "The encoder surface could not be attached.", true),
                )
            }
        }

        fun detachEncoderSurface() {
            display?.surface = null
            if (projection != null) {
                ProjectionSessionRegistry.update(CaptureState.AuthorizedAwaitingEncoder)
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTIFICATION_ID, notification("Capture authorization active"))

        if (intent?.action != ACTION_START) {
            ProjectionSessionRegistry.update(CaptureState.Failed("Invalid capture service request.", false))
            stopSelf()
            return START_NOT_STICKY
        }

        val resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, ActivityResultCodes.CANCELED)
        val resultData = intent.intentExtra(EXTRA_RESULT_DATA)
        if (resultCode != ActivityResultCodes.OK || resultData == null) {
            ProjectionSessionRegistry.update(CaptureState.Failed("Capture authorization data is missing.", true))
            stopSelf()
            return START_NOT_STICKY
        }

        releaseProjection()
        val manager = getSystemService(MediaProjectionManager::class.java)
        try {
            projection = manager.getMediaProjection(resultCode, resultData).also {
                it.registerCallback(callback, null)
            }
            ProjectionSessionRegistry.update(CaptureState.AuthorizedAwaitingEncoder)
        } catch (error: RuntimeException) {
            releaseProjection()
            ProjectionSessionRegistry.update(
                CaptureState.Failed(error.message ?: "Android rejected the capture authorization.", true),
            )
            stopSelf()
        }
        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        releaseProjection()
        super.onDestroy()
    }

    private fun releaseProjection(stopProjection: Boolean = true) {
        display?.release()
        display = null
        val activeProjection = projection
        projection = null
        activeProjection?.unregisterCallback(callback)
        if (stopProjection) activeProjection?.stop()
    }

    private fun notification(message: String): Notification = NotificationCompat.Builder(this, CHANNEL_ID)
        .setSmallIcon(android.R.drawable.ic_menu_view)
        .setContentTitle("UNKXWYNO Mirror")
        .setContentText(message)
        .setOngoing(true)
        .setOnlyAlertOnce(true)
        .setCategory(NotificationCompat.CATEGORY_SERVICE)
        .build()

    private fun createNotificationChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(CHANNEL_ID, "Screen capture", NotificationManager.IMPORTANCE_LOW),
        )
    }

    companion object {
        private const val ACTION_START = "io.unkxwyno.mirror.capture.START"
        private const val EXTRA_RESULT_CODE = "resultCode"
        private const val EXTRA_RESULT_DATA = "resultData"
        private const val CHANNEL_ID = "capture"
        private const val NOTIFICATION_ID = 4101

        fun start(context: Context, resultCode: Int, data: Intent) {
            val intent = Intent(context, CaptureForegroundService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_RESULT_CODE, resultCode)
                putExtra(EXTRA_RESULT_DATA, data)
            }
            ContextCompat.startForegroundService(context, intent)
        }
    }
}

private object ActivityResultCodes {
    const val OK = -1
    const val CANCELED = 0
}

private fun Intent.intentExtra(name: String): Intent? = if (Build.VERSION.SDK_INT >= 33) {
    getParcelableExtra(name, Intent::class.java)
} else {
    @Suppress("DEPRECATION")
    getParcelableExtra(name)
}
