package io.unkxwyno.mirror.codec

import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaFormat

data class VideoCodecCapability(
    val mimeType: String,
    val encoder: Boolean,
    val hardwareAccelerated: Boolean,
    val supportedProfiles: List<Int>,
    val supportedStandardModes: List<VideoMode>,
)

data class VideoMode(val width: Int, val height: Int, val framesPerSecond: Int)

object CodecCapabilityProbe {
    private val codecs = listOf(
        MediaFormat.MIMETYPE_VIDEO_AVC,
        MediaFormat.MIMETYPE_VIDEO_HEVC,
        MediaFormat.MIMETYPE_VIDEO_AV1,
    )

    private val modes = listOf(
        VideoMode(1280, 720, 30), VideoMode(1280, 720, 60),
        VideoMode(1920, 1080, 30), VideoMode(1920, 1080, 60),
        VideoMode(2560, 1440, 60), VideoMode(2560, 1440, 120),
        VideoMode(3840, 2160, 60), VideoMode(3840, 2160, 120),
    )

    fun probe(): List<VideoCodecCapability> = MediaCodecList(MediaCodecList.REGULAR_CODECS)
        .codecInfos
        .asSequence()
        .flatMap { info ->
            codecs.asSequence()
                .filter { mime -> info.supportedTypes.any { it.equals(mime, ignoreCase = true) } }
                .mapNotNull { mime -> capability(info, mime) }
        }
        .toList()

    private fun capability(info: MediaCodecInfo, mime: String): VideoCodecCapability? = runCatching {
        val capabilities = info.getCapabilitiesForType(mime)
        val video = capabilities.videoCapabilities
        VideoCodecCapability(
            mimeType = mime,
            encoder = info.isEncoder,
            hardwareAccelerated = info.isHardwareAccelerated,
            supportedProfiles = capabilities.profileLevels.map { it.profile }.distinct().sorted(),
            supportedStandardModes = modes.filter {
                video.isSizeSupported(it.width, it.height) &&
                    video.areSizeAndRateSupported(it.width, it.height, it.framesPerSecond.toDouble())
            },
        )
    }.getOrNull()
}
