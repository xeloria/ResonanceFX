// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import android.os.Build
import io.github.resonancefx.service.EngineService

/** Everything the status screen shows, gathered from the platform and the live engine. */
data class DriverStatus(
    val registered: Boolean,
    val engineName: String?,
    val implementor: String?,
    val attached: Boolean,
    val versionName: String?,
    val versionCode: Int?,
    val architecture: String?,
    val sampleRate: Int?,
    val channels: Int?,
    val configured: Boolean?,
    val processing: Boolean?,
    val kernelLoaded: Boolean?,
    val features: Int?,
    val disableReason: String?,
    val sessionError: String?,
    val assetError: String?,
) {
    /** One-line verdict. */
    val headline: String
        get() = when {
            !registered -> "Engine not loaded"
            !attached -> "Engine loaded · not attached"
            configured == false -> "Engine attached · stream rejected"
            processing == true -> "Processing audio"
            else -> "Ready · waiting for audio"
        }

    val healthy get() = registered && attached && configured != false && sessionError == null

    fun report(): String = buildString {
        appendLine("ResonanceFX status report")
        appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE}), Android ${Build.VERSION.RELEASE} / API ${Build.VERSION.SDK_INT}")
        appendLine("ABIs: ${Build.SUPPORTED_ABIS.joinToString()}")
        appendLine("Registered: $registered ($engineName / $implementor)")
        appendLine("Attached: $attached")
        appendLine("Driver: $versionName ($versionCode) $architecture")
        appendLine("Stream: ${sampleRate ?: "?"} Hz, ${channels ?: "?"} ch, configured=$configured, processing=$processing")
        appendLine("Convolver kernel loaded: $kernelLoaded")
        disableReason?.let { appendLine("Disable reason: $it") }
        sessionError?.let { appendLine("Session error: $it") }
        assetError?.let { appendLine("Asset error: $it") }
    }

    companion object {
        /** Blocking (binder calls); run off the main thread. */
        fun query(): DriverStatus {
            val descriptor = EngineEffect.descriptor()
            val manager = EngineService.engine
            val live = manager?.primary
            // With no live instance, briefly attach to read driver facts.
            val probe = if (live == null && descriptor != null) runCatching { EngineEffect(0, -1) }.getOrNull() else null
            val fx = live ?: probe
            try {
                val reason = fx?.getInt(Params.GET_DISABLE_REASON)
                return DriverStatus(
                    registered = descriptor != null,
                    engineName = descriptor?.name,
                    implementor = descriptor?.implementor,
                    attached = live != null,
                    versionName = fx?.getString(Params.GET_DRIVER_VERSION_NAME),
                    versionCode = fx?.getInt(Params.GET_DRIVER_VERSION_CODE),
                    architecture = fx?.getString(Params.GET_ARCHITECTURE),
                    sampleRate = live?.getInt(Params.GET_SAMPLING_RATE)?.takeIf { it > 0 },
                    channels = live?.getInt(Params.GET_CHANNEL_COUNT)?.takeIf { it > 0 },
                    configured = live?.getInt(Params.GET_CONFIGURE)?.let { it == 1 },
                    processing = live?.getInt(Params.GET_STREAMING)?.let { it == 1 },
                    kernelLoaded = live?.getInt(Params.GET_CONVOLUTION_KERNEL_ID)?.let { it != 0 },
                    features = fx?.getInt(Params.GET_FEATURES),
                    disableReason = if (live != null && reason != null && reason != 0)
                        live.getString(Params.GET_DISABLE_REASON_MESSAGE)?.ifBlank { null } ?: "code $reason" else null,
                    sessionError = manager?.error?.value,
                    assetError = EngineService.engineApplier?.lastAssetError,
                )
            } finally {
                probe?.close()
            }
        }
    }
}
