// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.engine

import android.util.Log
import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Codec
import io.github.resonancefx.driver.EngineEffect
import io.github.resonancefx.driver.Params
import io.github.resonancefx.autoeq.AutoEqDatabase
import io.github.resonancefx.model.AutoEqControl
import io.github.resonancefx.model.BandsControl
import io.github.resonancefx.model.GraphicEqControl
import io.github.resonancefx.model.Section
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.model.Choice
import io.github.resonancefx.model.Control
import io.github.resonancefx.model.Equalizer
import io.github.resonancefx.model.FileControl
import io.github.resonancefx.model.Slider
import io.github.resonancefx.model.Toggle
import java.io.File

/** Pushes [Settings] into an engine instance, either fully or only the keys that changed. */
class Applier(
    private val resolveAsset: (FileControl.Kind, String) -> File?,
    private val autoEq: () -> AutoEqDatabase?,
) {

    /** Last problem loading a user file, surfaced on the status screen. */
    @Volatile var lastAssetError: String? = null
        private set

    fun applyAll(fx: EngineEffect, settings: Settings) {
        // Without this the engine never timestamps buffers and always reports "not processing".
        fx.set(Params.SET_UPDATE_STATUS, 1)
        val features = fx.getInt(Params.GET_FEATURES) ?: 0
        Catalog.sections.forEach { section ->
            // Older engines don't know the module's parameters at all.
            if (section.feature != 0 && features and section.feature == 0) return@forEach
            applySection(fx, section, settings)
            if (section.enableKey != null && section.enableParam != null) {
                fx.set(section.enableParam, settings.bool(section.enableKey))
            }
        }
        fx.set(Params.SET_RESET_STATUS, 1)
        fx.enabled = settings.bool(Catalog.KEY_MASTER)
    }

    fun applyChanged(fx: EngineEffect, settings: Settings, changed: Set<String>) {
        val composed = HashSet<Section>()
        changed.forEach { key ->
            when {
                key == Catalog.KEY_MASTER -> fx.enabled = settings.bool(key)
                key in Catalog.sectionsByEnableKey -> {
                    val section = Catalog.sectionsByEnableKey.getValue(key)
                    // Re-send the module's values first: some modules reset state on enable.
                    if (settings.bool(key)) applySection(fx, section, settings)
                    fx.set(section.enableParam!!, settings.bool(key))
                    composed += section
                }
                key in Catalog.composites -> {
                    val section = Catalog.composites.getValue(key)
                    if (composed.add(section)) section.compose!!(fx, settings)
                }
                else -> Catalog.controlsByKey[key]?.let { (_, control) -> applyControl(fx, control, settings) }
            }
        }
        // Strength has no parameter of its own: it rescales the whole AutoEQ profile.
        if (Catalog.KEY_AUTOEQ_STRENGTH in changed && Catalog.KEY_AUTOEQ_PROFILE !in changed) applyAutoEq(fx, settings)
    }

    private fun applySection(fx: EngineEffect, section: Section, settings: Settings) {
        val compose = section.compose
        if (compose != null) compose(fx, settings) else section.controls.forEach { applyControl(fx, it, settings) }
    }

    private fun applyControl(fx: EngineEffect, control: Control, settings: Settings) {
        when (control) {
            is Toggle -> control.apply(fx, settings.bool(control.key))
            is Slider -> control.apply(fx, settings.int(control.key))
            is Choice -> control.apply(fx, settings.int(control.key))
            is Equalizer -> parseBands(settings.string(control.key)).forEachIndexed { band, gain ->
                fx.set(Params.FIR_EQUALIZER_BAND_LEVEL, Codec.eqBand(band, gain))
            }
            is FileControl -> applyFile(fx, control, settings.string(control.key))
            is AutoEqControl -> applyAutoEq(fx, settings)
            // Only ever part of composite sections, sent by Section.compose.
            is GraphicEqControl, is BandsControl -> Unit
        }
    }

    private fun applyAutoEq(fx: EngineEffect, settings: Settings) {
        val id = settings.string(Catalog.KEY_AUTOEQ_PROFILE)
        val features = fx.getInt(Params.GET_FEATURES) ?: 0
        if (features and Params.FEATURE_PARAMETRIC_EQ == 0) {
            if (id.isNotBlank()) lastAssetError = "AutoEQ needs the ResonanceFX engine v1.0.4 or newer — update the module"
            return
        }
        val profile = id.takeIf { it.isNotBlank() }?.let { autoEq()?.find(it) }
        if (profile == null) {
            fx.set(Params.PARAMETRIC_EQ_FILTERS, Codec.parametricEq(0f, emptyList()))
            if (id.isNotBlank()) lastAssetError = "AutoEQ: profile \"$id\" not found"
            return
        }
        val (preamp, filters) = profile.scaled(settings.int(Catalog.KEY_AUTOEQ_STRENGTH) / 100f)
        fx.set(
            Params.PARAMETRIC_EQ_FILTERS,
            Codec.parametricEq(preamp, filters.map { floatArrayOf(it.type.toFloat(), it.frequency, it.gainDb, it.q) }),
        )
        // A profile that now applies supersedes any earlier AutoEQ problem.
        if (lastAssetError?.startsWith("AutoEQ") == true) lastAssetError = null
    }

    private fun applyFile(fx: EngineEffect, control: FileControl, name: String) {
        val file = resolveAsset(control.kind, name)
        try {
            when (control.kind) {
                FileControl.Kind.DDC -> {
                    if (file == null) return
                    fx.set(Params.DDC_COEFFICIENTS, Codec.ddc(Assets.parseVdc(file.readText())))
                }
                FileControl.Kind.KERNEL -> {
                    if (file == null) {
                        fx.clearKernel()
                        return
                    }
                    val rate = fx.getInt(Params.GET_SAMPLING_RATE)?.takeIf { it in 8000..384000 } ?: 48000
                    val kernel = Assets.resample(Assets.parseWav(file.readBytes()), rate)
                    // Rate is part of the identity: a new output rate must trigger a re-upload.
                    fx.uploadKernel("$name@$rate", kernel.channels, kernel.samples)
                }
            }
            lastAssetError = null
        } catch (e: Exception) {
            lastAssetError = "${control.label}: ${e.message}"
            Log.w(TAG, "Failed to apply $name", e)
        }
    }

    companion object {
        private const val TAG = "Applier"

        fun parseBands(value: String): FloatArray {
            val gains = value.split(';').mapNotNull { it.trim().toFloatOrNull() }
            return FloatArray(10) { gains.getOrElse(it) { 0f }.coerceIn(-12f, 12f) }
        }

        fun formatBands(gains: FloatArray) = gains.joinToString(";") { "%.1f".format(java.util.Locale.ROOT, it) }
    }
}
