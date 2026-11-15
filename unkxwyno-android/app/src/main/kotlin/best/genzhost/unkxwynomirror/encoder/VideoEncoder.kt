/**
 * @file VideoEncoder.kt
 * @brief Hardware-accelerated low-latency H.264/AVC screen encoder via MediaCodec & Surface.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror.encoder

import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.projection.MediaProjection
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.Surface
import best.genzhost.unkxwynomirror.protocol.PacketFraming
import java.io.OutputStream
import java.util.concurrent.atomic.AtomicBoolean

class VideoEncoder(
    private val mediaProjection: MediaProjection,
    private val outputStream: OutputStream,
    private val width: Int = 1920,
    private val height: Int = 1080,
    private val dpi: Int = 320,
    private val bitrateBps: Int = 20_000_000,
    private val fps: Int = 60
) {
    private val TAG = "VideoEncoder"
    private val isRunning = AtomicBoolean(false)
    private var encoderThread: Thread? = null

    private var codec: MediaCodec? = null
    private var inputSurface: Surface? = null
    private var virtualDisplay: VirtualDisplay? = null
    private var sequenceCounter: Byte = 0

    fun start() {
        if (isRunning.getAndSet(true)) return

        encoderThread = Thread({
            try {
                runEncoderLoop()
            } catch (e: Exception) {
                Log.e(TAG, "Video encoder error: ${e.message}", e)
            } finally {
                release()
            }
        }, "unkXwyno-VideoEncoder").apply {
            priority = Thread.MAX_PRIORITY
            start()
        }
    }

    fun stop() {
        isRunning.set(false)
        encoderThread?.interrupt()
        try {
            encoderThread?.join(1000)
        } catch (_: InterruptedException) {}
        encoderThread = null
    }

    fun requestKeyframe() {
        try {
            val params = Bundle().apply {
                putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0)
            }
            codec?.setParameters(params)
            Log.d(TAG, "Immediate IDR keyframe requested.")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to request IDR keyframe: ${e.message}")
        }
    }

    private fun runEncoderLoop() {
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height).apply {
            setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
            setInteger(MediaFormat.KEY_BIT_RATE, bitrateBps)
            setInteger(MediaFormat.KEY_FRAME_RATE, fps)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1) // 1s max keyframe period
            setInteger(MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR)
            setInteger(MediaFormat.KEY_PRIORITY, 0) // Realtime priority

            // Static screen refresh (resend frame if screen is not updating)
            setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, 1_000_000L / fps)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                setInteger(MediaFormat.KEY_LATENCY, 0) // Zero-latency pipeline
            }
        }

        codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
        codec!!.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        inputSurface = codec!!.createInputSurface()
        codec!!.start()

        virtualDisplay = mediaProjection.createVirtualDisplay(
            "unkXwyno-VirtualDisplay",
            width, height, dpi,
            DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
            inputSurface, null, null
        )

        Log.i(TAG, "MediaCodec AVC encoder started: ${width}x${height} @ ${fps}fps, ${bitrateBps / 1_000_000}Mbps")

        val bufferInfo = MediaCodec.BufferInfo()
        val tempBuffer = ByteArray(2 * 1024 * 1024)

        while (isRunning.get()) {
            val outputBufferIndex = codec!!.dequeueOutputBuffer(bufferInfo, 10_000)

            if (outputBufferIndex >= 0) {
                val outputBuffer = codec!!.getOutputBuffer(outputBufferIndex) ?: continue

                if (bufferInfo.size > 0) {
                    outputBuffer.position(bufferInfo.offset)
                    outputBuffer.limit(bufferInfo.offset + bufferInfo.size)

                    val packetType = when {
                        (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0 -> PacketFraming.VIDEO_CONFIG
                        (bufferInfo.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0 -> PacketFraming.VIDEO_KEYFRAME
                        else -> PacketFraming.VIDEO_DELTA
                    }

                    val length = bufferInfo.size
                    val payload = if (length <= tempBuffer.size) {
                        outputBuffer.get(tempBuffer, 0, length)
                        tempBuffer
                    } else {
                        val bigBuf = ByteArray(length)
                        outputBuffer.get(bigBuf)
                        bigBuf
                    }

                    PacketFraming.writePacket(
                        outputStream,
                        PacketFraming.CHANNEL_VIDEO,
                        packetType,
                        sequenceCounter++,
                        payload,
                        0,
                        length
                    )
                }

                codec!!.releaseOutputBuffer(outputBufferIndex, false)
            }
        }
    }

    private fun release() {
        try {
            virtualDisplay?.release()
        } catch (_: Exception) {}
        try {
            inputSurface?.release()
        } catch (_: Exception) {}
        try {
            codec?.stop()
            codec?.release()
        } catch (_: Exception) {}

        virtualDisplay = null
        inputSurface = null
        codec = null
        Log.i(TAG, "Video encoder resources released.")
    }
}
