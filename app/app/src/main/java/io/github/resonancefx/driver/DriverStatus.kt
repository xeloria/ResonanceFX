// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import android.os.Build
import io.github.resonancefx.R
import io.github.resonancefx.service.EngineService
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

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
    val factory: String?,
    val disableReason: String?,
    val sessionError: String?,
    val assetError: String?,
) {
    /** One-line verdict, as a string resource. */
    val headlineRes: Int
        get() = when {
            !registered -> R.string.headline_not_loaded
            !attached -> R.string.headline_not_attached
            configured == false -> R.string.headline_stream_rejected
            processing == true -> R.string.headline_processing
            else -> R.string.headline_ready
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
        appendLine("Effect factory: ${factory ?: "not installed"}")
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
                    factory = detectFactory(),
                    disableReason = if (live != null && reason != null && reason != 0)
                        live.getString(Params.GET_DISABLE_REASON_MESSAGE)?.ifBlank { null } ?: "code $reason" else null,
                    sessionError = manager?.error?.value,
                    assetError = EngineService.engineApplier?.lastAssetError,
                )
            } finally {
                probe?.close()
            }
        }

        /**
         * Which effect-factory generation the module had to target, read back from the config it
         * patched (the app cannot write /vendor itself; the module applies it as root). AIDL
         * configs live in audio_effects_config*.xml, HIDL/legacy ones in audio_effects*.xml.
         */
        private fun detectFactory(): String? {
            fun carries(path: String): Boolean = runCatching {
                File(path).readText().contains(Params.UUID_ENGINE.toString())
            }.getOrDefault(false)
            // The framework parses the first readable config in this order (system/media
            // audio_config.h): /odm/etc, /vendor/etc/audio/sku_<ro.boot.product.vendor.sku>,
            // /vendor/etc, /system/etc. Scan the SKU directories and the framework copy too:
            // QTI ROMs often keep only the SKU config, and a ROM without a vendor copy makes the
            // factory (and AudioPolicy) fall back to /system.
            val skuDirs = File("/vendor/etc/audio").listFiles()
                ?.filter { it.isDirectory && it.name.startsWith("sku_") }
                .orEmpty()
                .map { it.path }
            val aidl = (listOf(
                "/odm/etc/audio_effects_config.xml",
                "/vendor/etc/audio_effects_config.xml",
                "/vendor/etc/audio_effects_config_sec.xml",
                "/system/etc/audio_effects_config.xml",
            ) + skuDirs.map { "$it/audio_effects_config.xml" }).any(::carries)
            val legacy = (listOf(
                "/odm/etc/audio_effects.xml",
                "/vendor/etc/audio_effects.xml",
                "/vendor/etc/audio_effects_sec.xml",
                "/vendor/etc/audio_effects_spatializer.xml",
                "/system/etc/audio_effects.xml",
            ) + skuDirs.map { "$it/audio_effects.xml" }).any(::carries)
            return when {
                aidl && legacy -> "AIDL + legacy"
                aidl -> "AIDL"
                legacy -> "HIDL/legacy"
                else -> null
            }
        }
    }
}

/**
 * Driver status as seen by the UI. The foreground service publishes a fresh snapshot from its
 * own watchdog tick (that thread already talks to the engine); the Status/Effects screens only
 * read this. The previous per-screen poll loops fired bursts of GET_PARAMETER commands against
 * the effect handle every couple of seconds, and on a suspended/offloaded sound output (e.g.
 * idle Bluetooth) those periodic chain-lock round trips were audible as a short stutter.
 */
object DriverStatusHub {
    private val _status = MutableStateFlow<DriverStatus?>(null)
    val status: StateFlow<DriverStatus?> = _status.asStateFlow()

    @Volatile var publishedAt: Long = 0
        private set

    /** Blocking probe + publish; call off the UI thread. */
    fun refresh() {
        _status.value = runCatching { DriverStatus.query() }.getOrNull()
        publishedAt = System.currentTimeMillis()
    }
}
