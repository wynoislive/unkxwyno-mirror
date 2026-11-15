/**
 * @file AudioCaptureManager.kt
 * @brief High-fidelity internal device audio capture via AudioPlaybackCapture API (Android 10+).
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror.audio

import android.annotation.SuppressLint
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.projection.MediaProjection
import android.os.Build
import android.util.Log
import best.genzhost.unkxwynomirror.protocol.PacketFraming
import java.io.OutputStream
import java.util.concurrent.atomic.AtomicBoolean

class AudioCaptureManager(
    private val mediaProjection: MediaProjection,
    private val outputStream: OutputStream,
    private val sampleRate: Int = 48000
) {
    private val TAG = "AudioCaptureManager"
    private val isRunning = AtomicBoolean(false)
    private var audioThread: Thread? = null
    private var audioRecord: AudioRecord? = null
    private var sequenceCounter: Byte = 0

    fun start() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            Log.w(TAG, "Internal audio capture requires Android 10 (API 29) or higher.")
            return
        }

        if (isRunning.getAndSet(true)) return

        audioThread = Thread({
            try {
                runAudioCaptureLoop()
            } catch (e: Exception) {
                Log.e(TAG, "Audio capture error: ${e.message}", e)
            } finally {
                release()
            }
        }, "unkXwyno-AudioCapture").apply {
            priority = Thread.MAX_PRIORITY
            start()
        }
    }

    fun stop() {
        isRunning.set(false)
        audioThread?.interrupt()
        try {
            audioThread?.join(1000)
        } catch (_: InterruptedException) {}
        audioThread = null
    }

    @SuppressLint("MissingPermission")
    private fun runAudioCaptureLoop() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return

        val config = AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
            .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
            .addMatchingUsage(AudioAttributes.USAGE_GAME)
            .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
            .build()

        val format = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(sampleRate)
            .setChannelMask(AudioFormat.CHANNEL_IN_STEREO)
            .build()

        val minBufSize = AudioRecord.getMinBufferSize(
            sampleRate,
            AudioFormat.CHANNEL_IN_STEREO,
            AudioFormat.ENCODING_PCM_16BIT
        )
        val bufferSize = (minBufSize * 2).coerceAtLeast(4096)

        audioRecord = AudioRecord.Builder()
            .setAudioPlaybackCaptureConfig(config)
            .setAudioFormat(format)
            .setBufferSizeInBytes(bufferSize)
            .build()

        if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
            Log.e(TAG, "AudioRecord failed to initialize.")
            return
        }

        audioRecord?.startRecording()
        Log.i(TAG, "Internal audio recording started at ${sampleRate}Hz 16-bit stereo PCM.")

        // Buffer for 10ms of 48kHz stereo 16-bit PCM (480 samples * 4 bytes = 1920 bytes)
        val chunkBytes = (sampleRate * 4 * 10) / 1000
        val pcmChunk = ByteArray(chunkBytes)

        while (isRunning.get()) {
            val bytesRead = audioRecord?.read(pcmChunk, 0, pcmChunk.size) ?: -1
            if (bytesRead > 0) {
                PacketFraming.writePacket(
                    outputStream,
                    PacketFraming.CHANNEL_AUDIO,
                    PacketFraming.AUDIO_PCM_S16LE,
                    sequenceCounter++,
                    pcmChunk,
                    0,
                    bytesRead
                )
            }
        }
    }

    private fun release() {
        try {
            audioRecord?.stop()
            audioRecord?.release()
        } catch (_: Exception) {}
        audioRecord = null
        Log.i(TAG, "Audio capture resources released.")
    }
}
