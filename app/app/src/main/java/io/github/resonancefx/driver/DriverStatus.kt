// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import android.os.Build
import io.github.resonancefx.App
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
    val diagnosis: Diagnosis,
    val moduleDiagnosis: ModuleDiagnosis?,
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

    /** The module's authoritative code when it published one, else the app-side subset. */
    val diagnosisCode: String get() = moduleDiagnosis?.code ?: diagnosis.code

    fun report(): String = buildString {
        appendLine("ResonanceFX status report")
        appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE}), Android ${Build.VERSION.RELEASE} / API ${Build.VERSION.SDK_INT}")
        appendLine("ABIs: ${Build.SUPPORTED_ABIS.joinToString()}")
        appendLine("Registered: $registered ($engineName / $implementor)")
        appendLine("Attached: $attached")
        appendLine("Driver: $versionName ($versionCode) $architecture")
        appendLine("Stream: ${sampleRate ?: "?"} Hz, ${channels ?: "?"} ch, configured=$configured, processing=$processing")
        appendLine("Effect factory: ${factory ?: "not installed"}")
        val module = moduleDiagnosis
        if (module != null) {
            appendLine("Diagnosis (module ${module.moduleVersion ?: "?"}): ${module.code} ${module.short}")
            if (module.detail.isNotBlank()) appendLine("  detail: ${module.detail}")
            if (module.fix.isNotBlank()) appendLine("  fix: ${module.fix}")
        } else {
            appendLine("Diagnosis (app): ${diagnosis.code} ${diagnosis.reason}")
        }
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
                val configured = live?.getInt(Params.GET_CONFIGURE)?.let { it == 1 }
                val configs = configCandidates()
                val factory = detectFactory(configs)
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
                    configured = configured,
                    processing = live?.getInt(Params.GET_STREAMING)?.let { it == 1 },
                    kernelLoaded = live?.getInt(Params.GET_CONVOLUTION_KERNEL_ID)?.let { it != 0 },
                    features = fx?.getInt(Params.GET_FEATURES),
                    factory = factory,
                    diagnosis = Diagnosis.of(
                        registered = descriptor != null,
                        serviceRunning = manager != null,
                        attached = live != null,
                        configured = configured,
                        factory = factory,
                        configFound = configs.isNotEmpty(),
                        aidlEngine = hasAidlEngine(),
                    ),
                    moduleDiagnosis = ModuleDiagnosis.read(App.instance.filesDir),
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
         * Every config the framework or a vendor HAL might read: /odm/etc, the QTI SKU
         * directories, /vendor/etc and /system/etc. The framework parses the first readable one
         * in that order; the app cannot read ro.boot.product.vendor.sku, so it scans them all.
         */
        private fun configCandidates(): List<File> {
            val dirs = listOf("/odm/etc", "/vendor/etc", "/system/etc") +
                File("/vendor/etc/audio").listFiles()
                    ?.filter { it.isDirectory && it.name.startsWith("sku_") }
                    .orEmpty()
                    .map { it.path }
            return dirs.flatMap { dir ->
                File(dir).listFiles().orEmpty()
                    .filter { it.isFile && it.name.startsWith("audio_effects") }
            }.distinctBy { it.path }.sortedBy { it.path }
        }

        /** True when an AIDL engine build is visible on this device (any partition). */
        private fun hasAidlEngine(): Boolean = listOf(
            "/system/lib64/soundfx", "/system/lib/soundfx",
            "/vendor/lib64/soundfx", "/vendor/lib/soundfx",
        ).any { dir ->
            File(dir).listFiles()?.any { it.name.startsWith("libresonancefx_aidl") } == true
        }

        /**
         * Which effect-factory generation the module had to target, read back from the config it
         * patched (the app cannot write /vendor itself; the module applies it as root). AIDL
         * configs live in audio_effects_config*.xml, HIDL/legacy ones in audio_effects*.xml.
         */
        private fun detectFactory(candidates: List<File>): String? {
            fun carries(file: File): Boolean = runCatching {
                file.readText().contains(Params.UUID_ENGINE.toString())
            }.getOrDefault(false)
            // AIDL configs live in audio_effects_config*.xml, HIDL/legacy ones in
            // audio_effects*.xml. A definition repeated in several files is fine.
            val aidl = candidates.filter { it.name.startsWith("audio_effects_config") }.any(::carries)
            val legacy = candidates.filter { !it.name.startsWith("audio_effects_config") }.any(::carries)
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
