/**
 * @file MirroringService.kt
 * @brief Foreground Service coordinating USB AOA streaming, hardware video encoder, and internal audio.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.hardware.display.DisplayManager
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import best.genzhost.unkxwynomirror.MainActivity
import best.genzhost.unkxwynomirror.audio.AudioCaptureManager
import best.genzhost.unkxwynomirror.encoder.VideoEncoder
import best.genzhost.unkxwynomirror.protocol.PacketFraming
import best.genzhost.unkxwynomirror.usb.AoaAccessoryManager
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicBoolean

class MirroringService : Service() {
    private val TAG = "MirroringService"

    companion object {
        const val ACTION_START = "best.genzhost.unkxwynomirror.action.START"
        const val ACTION_STOP  = "best.genzhost.unkxwynomirror.action.STOP"

        const val EXTRA_RESULT_CODE = "extra_result_code"
        const val EXTRA_RESULT_DATA = "extra_result_data"
        const val EXTRA_WIDTH       = "extra_width"
        const val EXTRA_HEIGHT      = "extra_height"
        const val EXTRA_DPI         = "extra_dpi"
        const val EXTRA_BITRATE     = "extra_bitrate"
        const val EXTRA_FPS         = "extra_fps"
        const val EXTRA_AUDIO       = "extra_audio"

        private const val NOTIFICATION_CHANNEL_ID = "unkxwyno_mirror_channel"
        private const val NOTIFICATION_ID = 1337
    }

    private var aoaManager: AoaAccessoryManager? = null
    private var mediaProjection: MediaProjection? = null
    private var videoEncoder: VideoEncoder? = null
    private var audioCapture: AudioCaptureManager? = null

    private val isRunning = AtomicBoolean(false)
    private var controlThread: Thread? = null

    override fun onCreate() {
        super.onCreate()
        aoaManager = AoaAccessoryManager(this)
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val action = intent?.action ?: return START_NOT_STICKY

        when (action) {
            ACTION_START -> {
                val resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, 0)
                val resultData = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    intent.getParcelableExtra(EXTRA_RESULT_DATA, Intent::class.java)
                } else {
                    @Suppress("DEPRECATION")
                    intent.getParcelableExtra(EXTRA_RESULT_DATA)
                }

                if (resultCode != 0 && resultData != null) {
                    startForegroundNotification()
                    startStreaming(intent, resultCode, resultData)
                } else {
                    Log.e(TAG, "Invalid MediaProjection intent or result code.")
                    stopSelf()
                }
            }
            ACTION_STOP -> {
                stopStreaming()
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }

        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        stopStreaming()
        super.onDestroy()
    }

    private fun startStreaming(intent: Intent, resultCode: Int, resultData: Intent) {
        if (isRunning.getAndSet(true)) return

        val width = intent.getIntExtra(EXTRA_WIDTH, 1920)
        val height = intent.getIntExtra(EXTRA_HEIGHT, 1080)
        val dpi = intent.getIntExtra(EXTRA_DPI, 320)
        val bitrate = intent.getIntExtra(EXTRA_BITRATE, 20_000_000)
        val fps = intent.getIntExtra(EXTRA_FPS, 60)
        val enableAudio = intent.getBooleanExtra(EXTRA_AUDIO, true)

        val mpManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
        mediaProjection = mpManager.getMediaProjection(resultCode, resultData)

        // Try opening AOA Accessory
        val connected = aoaManager?.openAccessory() ?: false
        if (!connected || aoaManager?.outputStream == null) {
            Log.e(TAG, "Failed to connect to AOA accessory host.")
            stopSelf()
            return
        }

        val outStream = aoaManager!!.outputStream!!
        val inStream = aoaManager!!.inputStream!!

        // Initialize Video Encoder
        videoEncoder = VideoEncoder(
            mediaProjection = mediaProjection!!,
            outputStream = outStream,
            width = width,
            height = height,
            dpi = dpi,
            bitrateBps = bitrate,
            fps = fps
        ).apply { start() }

        // Initialize Audio Capture if requested
        if (enableAudio) {
            audioCapture = AudioCaptureManager(
                mediaProjection = mediaProjection!!,
                outputStream = outStream,
                sampleRate = 48000
            ).apply { start() }
        }

        // Start Control Packet Receiver Thread (Handles Ping, IDR Request, Bitrate changes)
        controlThread = Thread({
            runControlLoop(inStream)
        }, "unkXwyno-ControlReceiver").apply { start() }

        Log.i(TAG, "unkXwyno Mirroring Service running.")
    }

    private fun stopStreaming() {
        if (!isRunning.getAndSet(false)) return

        controlThread?.interrupt()
        controlThread = null

        audioCapture?.stop()
        audioCapture = null

        videoEncoder?.stop()
        videoEncoder = null

        try {
            mediaProjection?.stop()
        } catch (_: Exception) {}
        mediaProjection = null

        aoaManager?.closeAccessory()
        Log.i(TAG, "unkXwyno Mirroring Service stopped.")
    }

    private fun runControlLoop(inStream: InputStream) {
        val headerBuf = ByteArray(PacketFraming.HEADER_SIZE)
        val byteBuffer = ByteBuffer.wrap(headerBuf).order(ByteOrder.LITTLE_ENDIAN)

        while (isRunning.get()) {
            try {
                var readTotal = 0
                while (readTotal < PacketFraming.HEADER_SIZE) {
                    val r = inStream.read(headerBuf, readTotal, PacketFraming.HEADER_SIZE - readTotal)
                    if (r < 0) return
                    readTotal += r
                }

                byteBuffer.rewind()
                val bodyLen = byteBuffer.int
                val channelId = byteBuffer.get()
                val packetType = byteBuffer.get()
                val sequence = byteBuffer.get()

                // Skip or read payload
                if (bodyLen > 0) {
                    val payload = ByteArray(bodyLen)
                    var pRead = 0
                    while (pRead < bodyLen) {
                        val r = inStream.read(payload, pRead, bodyLen - pRead)
                        if (r < 0) return
                        pRead += r
                    }
                }

                // Process Control packets
                if (channelId == PacketFraming.CHANNEL_CONTROL) {
                    when (packetType) {
                        PacketFraming.CTRL_PING -> {
                            aoaManager?.outputStream?.let { out ->
                                PacketFraming.writePacket(
                                    out,
                                    PacketFraming.CHANNEL_CONTROL,
                                    PacketFraming.CTRL_PONG,
                                    sequence,
                                    ByteArray(0)
                                )
                            }
                        }
                        PacketFraming.CTRL_REQ_KEYFRAME -> {
                            videoEncoder?.requestKeyframe()
                        }
                    }
                }
            } catch (e: Exception) {
                if (isRunning.get()) {
                    Log.w(TAG, "Control loop read exception: ${e.message}")
                }
                break
            }
        }
    }

    private fun startForegroundNotification() {
        val notification = createNotification()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun createNotification(): Notification {
        val contentIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this, 1,
            Intent(this, MirroringService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setContentTitle("unkXwyno Mirror Active")
            .setContentText("Zero-latency streaming over USB AOA 2.0")
            .setSmallIcon(android.R.drawable.ic_menu_slideshow)
            .setContentIntent(contentIntent)
            .addAction(android.R.drawable.ic_media_pause, "Stop", stopIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                "unkXwyno Mirroring Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Shows persistent status while mirroring screen over USB"
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }
}
