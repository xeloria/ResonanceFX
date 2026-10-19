// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Encodes parameter payloads exactly as the engine's EFFECT_CMD_SET_PARAM handler expects. */
object Codec {
    fun int(value: Int): ByteArray =
        ByteBuffer.allocate(4).order(ByteOrder.nativeOrder()).putInt(value).array()

    fun ints(vararg values: Int): ByteArray {
        val buffer = ByteBuffer.allocate(values.size * 4).order(ByteOrder.nativeOrder())
        values.forEach { buffer.putInt(it) }
        return buffer.array()
    }

    fun readInt(bytes: ByteArray): Int = ByteBuffer.wrap(bytes).order(ByteOrder.nativeOrder()).getInt(0)

    /** DDC: [pair count][44.1 kHz coefficients..., 48 kHz coefficients...] padded to 1024 bytes. */
    fun ddc(coefficients: FloatArray): ByteArray {
        val size = maxOf(Params.DDC_PAYLOAD_BYTES, 4 + coefficients.size * 4)
        val buffer = ByteBuffer.allocate(size).order(ByteOrder.nativeOrder())
        buffer.putInt(coefficients.size / 2)
        coefficients.forEach { buffer.putFloat(it) }
        return buffer.array()
    }

    /** FIR EQ band: [band index][gain in 0.01 dB]. */
    fun eqBand(band: Int, gainDb: Float): ByteArray = ints(band, Math.round(gainDb * 100))

    /**
     * Convolution kernel upload: one PREPARE, then 8184-byte chunks each wrapped as
     * [kernel id][float count][data] padded to 8192 bytes, then a COMMIT carrying the CRCs.
     */
    fun kernelChunks(kernelId: Int, samples: ByteArray): List<ByteArray> {
        val chunks = ArrayList<ByteArray>()
        var offset = 0
        while (offset < samples.size) {
            val length = minOf(Params.KERNEL_CHUNK_DATA_BYTES, samples.size - offset)
            val buffer = ByteBuffer.allocate(Params.KERNEL_CHUNK_PAYLOAD_BYTES).order(ByteOrder.nativeOrder())
            buffer.putInt(kernelId)
            buffer.putInt(length / 4)
            buffer.put(samples, offset, length)
            chunks += buffer.array()
            offset += length
        }
        return chunks
    }

    /**
     * Parametric EQ: [filter count][preamp dB][type, Hz, dB, Q per filter] padded to 1024 bytes.
     * type: 0 peaking, 1 low shelf, 2 high shelf.
     */
    fun parametricEq(preampDb: Float, filters: List<FloatArray>): ByteArray {
        require(filters.size <= Params.PARAMETRIC_EQ_MAX_FILTERS) { "Too many filters" }
        val buffer = ByteBuffer.allocate(Params.DDC_PAYLOAD_BYTES).order(ByteOrder.nativeOrder())
        buffer.putInt(filters.size)
        buffer.putFloat(preampDb)
        filters.forEach { f -> f.forEach { buffer.putFloat(it) } }
        return buffer.array()
    }

    /** Generic array payload: [count][floats] padded to [size] bytes (256, 1024 or 8192). */
    fun floatArray(count: Int, values: FloatArray, size: Int = Params.DDC_PAYLOAD_BYTES): ByteArray {
        require(4 + values.size * 4 <= size) { "Payload too large" }
        val buffer = ByteBuffer.allocate(size).order(ByteOrder.nativeOrder())
        buffer.putInt(count)
        values.forEach { buffer.putFloat(it) }
        return buffer.array()
    }

    fun floatsToBytes(samples: FloatArray): ByteArray {
        val buffer = ByteBuffer.allocate(samples.size * 4).order(ByteOrder.nativeOrder())
        samples.forEach { buffer.putFloat(it) }
        return buffer.array()
    }
}
