/**
 * @file PacketFraming.kt
 * @brief Binary protocol packet serializer matching unkXwyno 8-byte wire framing.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror.protocol

import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

object PacketFraming {
    const val HEADER_SIZE = 8

    // Channel Identifiers
    const val CHANNEL_CONTROL: Byte = 0x00
    const val CHANNEL_VIDEO: Byte   = 0x01
    const val CHANNEL_AUDIO: Byte   = 0x02
    const val CHANNEL_INPUT: Byte   = 0x03

    // Control Types
    const val CTRL_PING: Byte             = 0x80.toByte()
    const val CTRL_PONG: Byte             = 0x81.toByte()
    const val CTRL_REQ_KEYFRAME: Byte     = 0x82.toByte()
    const val CTRL_SET_SCREEN_POWER: Byte = 0x86.toByte()

    // Video Types
    const val VIDEO_CONFIG: Byte   = 0x01 // SPS/PPS
    const val VIDEO_KEYFRAME: Byte = 0x02 // IDR
    const val VIDEO_DELTA: Byte    = 0x03 // P/B Frame

    // Audio Types
    const val AUDIO_CONFIG: Byte    = 0x10
    const val AUDIO_PCM_S16LE: Byte = 0x11

    /**
     * Serializes and writes a framed packet directly to an output stream.
     * Uses Little-Endian byte order matching PC native x86/ARM memory layouts.
     */
    @Synchronized
    fun writePacket(
        out: OutputStream,
        channelId: Byte,
        packetType: Byte,
        sequence: Byte,
        payload: ByteArray,
        offset: Int = 0,
        length: Int = payload.size
    ) {
        val header = ByteArray(HEADER_SIZE)
        val buf = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)

        buf.putInt(length)     // Bytes 0-3: Body length
        buf.put(channelId)     // Byte 4: Channel ID
        buf.put(packetType)    // Byte 5: Packet Type
        buf.put(sequence)      // Byte 6: Sequence counter
        buf.put(0x00.toByte()) // Byte 7: Reserved

        out.write(header)
        if (length > 0) {
            out.write(payload, offset, length)
        }
        out.flush()
    }
}
