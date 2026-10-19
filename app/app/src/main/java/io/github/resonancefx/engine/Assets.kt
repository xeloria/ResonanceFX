// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.engine

import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.min
import kotlin.math.sin

/** Parsers for user-imported DDC profiles and convolution impulse responses. */
object Assets {

    /**
     * A .vdc file holds two biquad cascades, one per supported rate:
     *   SR_44100:b0,b1,...
     *   SR_48000:b0,b1,...
     * The engine takes both lists back to back, preceded by the per-rate count.
     */
    fun parseVdc(text: String): FloatArray {
        var at441: List<Float>? = null
        var at48: List<Float>? = null
        text.lineSequence().map { it.trim() }.forEach { line ->
            val (tag, body) = line.split(':', limit = 2).takeIf { it.size == 2 } ?: return@forEach
            val values = body.split(',').map { it.trim() }.filter { it.isNotEmpty() }.map { it.toFloat() }
            when (tag.trim().uppercase()) {
                "SR_44100" -> at441 = values
                "SR_48000" -> at48 = values
            }
        }
        val a = requireNotNull(at441) { "Missing SR_44100 coefficients" }
        val b = requireNotNull(at48) { "Missing SR_48000 coefficients" }
        require(a.size == b.size && a.isNotEmpty()) { "Coefficient lists must be non-empty and the same length" }
        // The engine accepts this parameter only as a 1024-byte payload.
        require((a.size + b.size) * 4 + 4 <= 1024) { "Profile has too many coefficients for the engine" }
        return (a + b).toFloatArray()
    }

    class Kernel(val channels: Int, val sampleRate: Int, val samples: FloatArray) {
        val frames get() = samples.size / channels
    }

    /** Reads PCM 16/24/32-bit or 32-bit float WAV (incl. WAVE_FORMAT_EXTENSIBLE), 1, 2 or 4 channels. */
    fun parseWav(bytes: ByteArray): Kernel {
        val buf = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        require(bytes.size > 44 && tag(buf, 0) == "RIFF" && tag(buf, 8) == "WAVE") { "Not a WAV file" }

        var format = -1
        var channels = 0
        var sampleRate = 0
        var bits = 0
        var dataOffset = -1
        var dataSize = 0
        var pos = 12
        while (pos + 8 <= bytes.size) {
            val id = tag(buf, pos)
            val size = buf.getInt(pos + 4).toLong() and 0xFFFFFFFFL
            val body = pos + 8
            when (id) {
                "fmt " -> {
                    format = buf.getShort(body).toInt() and 0xFFFF
                    channels = buf.getShort(body + 2).toInt()
                    sampleRate = buf.getInt(body + 4)
                    bits = buf.getShort(body + 14).toInt()
                    if (format == 0xFFFE && size >= 26) format = buf.getShort(body + 24).toInt() and 0xFFFF
                }
                "data" -> {
                    dataOffset = body
                    dataSize = min(size, (bytes.size - body).toLong()).toInt()
                }
            }
            // Chunks are word aligned.
            pos = (body + size + (size and 1)).coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
        }

        require(dataOffset >= 0) { "WAV has no data chunk" }
        require(channels == 1 || channels == 2 || channels == 4) { "Impulse response must be mono, stereo or 4-channel (got $channels)" }
        require(sampleRate in 8000..384000) { "Unsupported sample rate $sampleRate" }
        val isFloat = format == 3
        require(format == 1 || isFloat) { "Unsupported WAV encoding $format" }
        require(if (isFloat) bits == 32 else bits == 16 || bits == 24 || bits == 32) { "Unsupported bit depth $bits" }

        val bytesPerSample = bits / 8
        val count = dataSize / bytesPerSample / channels * channels
        require(count / channels >= 16) { "Impulse response is too short" }
        val out = FloatArray(count)
        var p = dataOffset
        for (i in 0 until count) {
            out[i] = when {
                isFloat -> buf.getFloat(p)
                bits == 16 -> buf.getShort(p) / 32768f
                bits == 24 -> ((bytes[p].toInt() and 0xFF) or ((bytes[p + 1].toInt() and 0xFF) shl 8) or
                        (bytes[p + 2].toInt() shl 16)) / 8388608f
                else -> (buf.getInt(p) / 2147483648.0).toFloat()
            }
            p += bytesPerSample
        }
        return Kernel(channels, sampleRate, out)
    }

    /**
     * Band-limited (windowed-sinc) resampling, so an impulse response recorded at 44.1 kHz sounds
     * the same on a 48 kHz or 96 kHz output instead of being pitch/time shifted.
     */
    fun resample(kernel: Kernel, targetRate: Int): Kernel {
        if (targetRate <= 0 || targetRate == kernel.sampleRate) return kernel
        val ch = kernel.channels
        val ratio = targetRate.toDouble() / kernel.sampleRate
        val inFrames = kernel.frames
        val outFrames = maxOf(16, (inFrames * ratio).toInt())
        val cutoff = min(1.0, ratio)
        val half = 32
        val out = FloatArray(outFrames * ch)
        for (o in 0 until outFrames) {
            val t = o / ratio
            val center = floor(t).toInt()
            for (c in 0 until ch) {
                var acc = 0.0
                for (k in center - half + 1..center + half) {
                    if (k < 0 || k >= inFrames) continue
                    val x = t - k
                    val w = 0.42 + 0.5 * cos(PI * x / half) + 0.08 * cos(2 * PI * x / half) // Blackman
                    acc += kernel.samples[k * ch + c] * sinc(cutoff * x) * cutoff * w
                }
                out[o * ch + c] = acc.toFloat()
            }
        }
        return Kernel(ch, targetRate, out)
    }

    private fun sinc(x: Double) = if (x == 0.0) 1.0 else sin(PI * x) / (PI * x)

    private fun tag(buf: ByteBuffer, at: Int) = String(ByteArray(4) { buf.get(at + it) }, Charsets.US_ASCII)
}
