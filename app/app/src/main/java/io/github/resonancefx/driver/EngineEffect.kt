// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import android.media.audiofx.AudioEffect
import android.util.Log
import java.lang.reflect.InvocationTargetException
import java.util.UUID
import java.util.zip.CRC32
import kotlin.random.Random

/**
 * One engine instance attached to an audio session. The AudioEffect(UUID, UUID, int, int)
 * constructor and the byte[] set/getParameter overloads are hidden SDK API (on the
 * "unsupported but allowed" list), so they're reached through reflection.
 */
class EngineEffect(val sessionId: Int, priority: Int = 0) : AutoCloseable {
    val effect: AudioEffect = try {
        constructor.newInstance(Params.TYPE_NULL, Params.UUID_ENGINE, priority, sessionId) as AudioEffect
    } catch (e: InvocationTargetException) {
        throw e.targetException
    }

    var enabled: Boolean
        get() = runCatching { effect.enabled }.getOrDefault(false)
        set(value) {
            runCatching { effect.enabled = value }.onFailure { Log.w(TAG, "setEnabled($value) failed", it) }
        }

    val hasControl: Boolean get() = runCatching { effect.hasControl() }.getOrDefault(false)

    fun set(param: Int, value: Int): Int = setRaw(Codec.int(param), Codec.int(value))

    fun set(param: Int, value: Boolean): Int = set(param, if (value) 1 else 0)

    fun set(param: Int, vararg values: Int): Int = setRaw(Codec.int(param), Codec.ints(*values))

    fun set(param: Int, payload: ByteArray): Int = setRaw(Codec.int(param), payload)

    fun getInt(param: Int): Int? {
        val reply = ByteArray(4)
        return if (getRaw(param, reply) >= 0) Codec.readInt(reply) else null
    }

    fun getString(param: Int): String? {
        val reply = ByteArray(128)
        val length = getRaw(param, reply)
        return if (length in 0..reply.size) String(reply, 0, length, Charsets.US_ASCII) else null
    }

    fun getRaw(param: Int, reply: ByteArray): Int = try {
        val (method, intKey) = getParameter
        method.invoke(effect, if (intKey) param else Codec.int(param), reply) as Int
    } catch (e: InvocationTargetException) {
        Log.w(TAG, "getParameter($param) failed", e.targetException)
        -1
    } catch (e: Exception) {
        Log.w(TAG, "getParameter($param) failed", e)
        -1
    }

    private fun setRaw(param: ByteArray, value: ByteArray): Int = try {
        val (method, intKey) = setParameter
        method.invoke(effect, if (intKey) Codec.readInt(param) else param, value) as Int
    } catch (e: InvocationTargetException) {
        Log.w(TAG, "setParameter failed", e.targetException)
        -1
    } catch (e: Exception) {
        Log.w(TAG, "setParameter failed", e)
        -1
    }

    /**
     * Uploads a convolution kernel (interleaved float samples). Skipped when the engine already
     * holds this exact kernel, which it reports as the CRC32 of [name].
     */
    fun uploadKernel(name: String, channels: Int, samples: FloatArray) {
        val nameCrc = crc(name.toByteArray())
        if (getInt(Params.GET_CONVOLUTION_KERNEL_ID) == nameCrc) return
        val data = Codec.floatsToBytes(samples)
        val kernelId = Random.nextInt()
        set(Params.CONVOLUTION_PREPARE_BUFFER, kernelId, channels, 0)
        Codec.kernelChunks(kernelId, data).forEach { set(Params.CONVOLUTION_SET_BUFFER, it) }
        set(Params.CONVOLUTION_COMMIT_BUFFER, kernelId, crc(data), nameCrc)
    }

    fun clearKernel() {
        set(Params.CONVOLUTION_PREPARE_BUFFER, 0, 0, 1)
    }

    override fun close() {
        runCatching { effect.release() }
    }

    companion object {
        private const val TAG = "EngineEffect"

        private val constructor by lazy {
            AudioEffect::class.java.getConstructor(
                UUID::class.java, UUID::class.java, Int::class.javaPrimitiveType, Int::class.javaPrimitiveType,
            )
        }
        /**
         * The byte-array parameter calls are hidden API. Which overload an app may reflect on
         * depends on the Android release and target SDK, so take the first that resolves:
         * `(int, byte[])` (same wire format, psize 4), then `(byte[], byte[])`, and only then
         * lift the hidden-API restriction for this process and retry.
         * Second value: true when the parameter key is passed as an int.
         */
        private val setParameter: Pair<java.lang.reflect.Method, Boolean> by lazy { resolve("setParameter") }
        private val getParameter: Pair<java.lang.reflect.Method, Boolean> by lazy { resolve("getParameter") }

        private fun resolve(name: String): Pair<java.lang.reflect.Method, Boolean> {
            fun find(): Pair<java.lang.reflect.Method, Boolean>? =
                runCatching { AudioEffect::class.java.getMethod(name, Int::class.javaPrimitiveType, ByteArray::class.java) to true }.getOrNull()
                    ?: runCatching { AudioEffect::class.java.getMethod(name, ByteArray::class.java, ByteArray::class.java) to false }.getOrNull()
            find()?.let { Log.i(TAG, "Using AudioEffect.$name(${if (it.second) "int" else "byte[]"}, byte[])"); return it }
            HiddenApi.unseal()
            return find()?.also { Log.i(TAG, "Using AudioEffect.$name after hidden-API exemption") }
                ?: throw NoSuchMethodException("AudioEffect.$name is not accessible on this Android build")
        }

        private fun crc(bytes: ByteArray): Int = CRC32().apply { update(bytes) }.value.toInt()

        /** True if the platform's effect factory knows the engine UUID (i.e. the module loaded). */
        fun isRegistered(): Boolean = descriptor() != null

        fun descriptor(): AudioEffect.Descriptor? =
            runCatching { AudioEffect.queryEffects()?.firstOrNull { it.uuid == Params.UUID_ENGINE } }.getOrNull()
    }
}
