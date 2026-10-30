// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.model

import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Codec
import java.util.Locale

/** Payloads for sections whose engine parameter combines several controls. */
object Composite {

    /** One user parametric EQ band; [type] as in [BandsControl]. */
    data class Band(val type: Int, val frequency: Float, val gainDb: Float, val q: Float)

    val BAND_TYPES = listOf("Peak", "Low shelf", "High shelf", "Low-pass", "High-pass", "Band-pass", "Notch")
    const val MAX_BANDS = 32

    fun parseBands(value: String): List<Band> = value.split(';').mapNotNull { entry ->
        val f = entry.split(',').map { it.trim().toFloatOrNull() ?: return@mapNotNull null }
        if (f.size != 4) return@mapNotNull null
        Band(
            f[0].toInt().coerceIn(BAND_TYPES.indices),
            f[1].coerceIn(20f, 20000f),
            f[2].coerceIn(-24f, 24f),
            f[3].coerceIn(0.1f, 10f),
        )
    }.take(MAX_BANDS)

    fun formatBands(bands: List<Band>) = bands.joinToString(";") {
        String.format(Locale.ROOT, "%d,%.1f,%.1f,%.2f", it.type, it.frequency, it.gainDb, it.q)
    }

    /** Band centres of the engine's graphic EQ layouts (1 = 15 bands, 2 = 31 bands). */
    fun graphicEqBands(layout: Int): List<Float> = if (layout == 1) {
        listOf(25f, 40f, 63f, 100f, 160f, 250f, 400f, 630f, 1000f, 1600f, 2500f, 4000f, 6300f, 10000f, 16000f)
    } else {
        listOf(
            20f, 25f, 31.5f, 40f, 50f, 63f, 80f, 100f, 125f, 160f, 200f, 250f, 315f, 400f, 500f, 630f,
            800f, 1000f, 1250f, 1600f, 2000f, 2500f, 3150f, 4000f, 5000f, 6300f, 8000f, 10000f, 12500f,
            16000f, 20000f,
        )
    }

    /** Gains for [count] bands; missing entries are 0 dB. */
    fun parseGains(value: String, count: Int): FloatArray {
        val gains = value.split(';').mapNotNull { it.trim().toFloatOrNull() }
        return FloatArray(count) { gains.getOrElse(it) { 0f }.coerceIn(-12f, 12f) }
    }

    fun formatGains(gains: FloatArray) = gains.joinToString(";") { String.format(Locale.ROOT, "%.1f", it) }

    /**
     * A speaker measurement: the correction fitted to it (preamp + bands) and the measured
     * response on [io.github.resonancefx.autoeq.Response.frequencies], normalised to 0 dB in the
     * midrange. Stored as "preamp;lowHz;highHz|bands|r0,r1,...".
     */
    data class Measurement(
        val preampDb: Float,
        val bands: List<Band>,
        val responseDb: List<Float>,
        val lowHz: Float,
        val highHz: Float,
        val distortion: Distortion? = null,
    )

    /**
     * Where the speaker starts to distort, per band of the engine's measured protection: the
     * loudest sweep level (dBFS) each band played below 5 % THD, or null if it stayed clean at
     * every level tried. [volumeDb] is the gain Android applied before the effects at the time
     * (the volume on the phone's speaker, 0 dB on Bluetooth), turning those levels into what the
     * engine sees.
     */
    data class Distortion(val volumeDb: Float, val cleanUpToDb: List<Float?>) {
        /** Engine ceilings (dBFS peak, 0 = no limit) for the measured protection mode. */
        fun ceilings(): FloatArray = FloatArray(cleanUpToDb.size) { i ->
            cleanUpToDb[i]?.let { (it + volumeDb).coerceIn(-60f, -0.1f) } ?: 0f
        }
    }

    fun parseMeasurement(value: String): Measurement? {
        val parts = value.split('|')
        if (parts.size != 3 && parts.size != 4) return null
        val head = parts[0].split(';').map { it.toFloatOrNull() ?: return null }
        if (head.size != 3) return null
        val response = parts[2].split(',').map { it.toFloatOrNull() ?: return null }
        val distortion = parts.getOrNull(3)?.split(';')?.let { d ->
            val volume = d.firstOrNull()?.toFloatOrNull() ?: return null
            Distortion(volume, d.drop(1).map { it.toFloatOrNull() })
        }
        return Measurement(head[0].coerceIn(-24f, 0f), parseBands(parts[1]), response, head[1], head[2], distortion)
    }

    fun formatMeasurement(m: Measurement): String =
        String.format(Locale.ROOT, "%.2f;%.0f;%.0f", m.preampDb, m.lowHz, m.highHz) + "|" + formatBands(m.bands) + "|" +
            m.responseDb.joinToString(",") { String.format(Locale.ROOT, "%.1f", it) } +
            (m.distortion?.let { d ->
                "|" + String.format(Locale.ROOT, "%.1f", d.volumeDb) + ";" +
                    d.cleanUpToDb.joinToString(";") { c -> c?.let { String.format(Locale.ROOT, "%.1f", it) } ?: "-" }
            } ?: "")

    /** The measured-protection ceilings payload, or null without a distortion measurement. */
    fun protectionCeilings(value: String): ByteArray? {
        val d = parseMeasurement(value)?.distortion ?: return null
        val c = d.ceilings()
        return Codec.floatArray(c.size, c)
    }

    /** The measured correction as the engine's EQ payload (empty without a measurement). */
    fun speakerCorrection(value: String): ByteArray {
        val m = parseMeasurement(value) ?: return Codec.parametricEq(0f, emptyList())
        return Codec.parametricEq(m.preampDb, m.bands.map { floatArrayOf(it.type.toFloat(), it.frequency, it.gainDb, it.q) })
    }

    /**
     * Multiband compressor from the simple controls: shared threshold and ratio, crossovers
     * spread over the spectrum, and the lowest band gentler so bass doesn't pump.
     */
    fun multiband(s: Settings): ByteArray {
        val bands = s.int("mbc.bands") + 3
        val crossovers = when (bands) {
            3 -> floatArrayOf(200f, 3000f, 8000f, 14000f)
            4 -> floatArrayOf(150f, 1000f, 5000f, 14000f)
            else -> floatArrayOf(120f, 500f, 2000f, 7000f)
        }
        val threshold = s.int("mbc.threshold").toFloat()
        val ratio = s.int("mbc.ratio") / 10f
        // Speed 0..100 % maps attack 40..2 ms and release 400..60 ms (lowest band twice as slow).
        val speed = s.int("mbc.speed") / 100f
        val attack = 40f - 38f * speed
        val release = 400f - 340f * speed
        val v = ArrayList<Float>(33)
        v += bands.toFloat()
        v += 6f
        v += s.int("mbc.mix") / 100f
        v += if (s.bool("mbc.makeup")) 1f else 0f
        crossovers.forEach { v += it }
        repeat(5) { v += if (it == 0) threshold + 3f else threshold }
        repeat(5) { v += if (it == 0) 1f + (ratio - 1f) * 0.75f else ratio }
        repeat(5) { v += if (it == 0) attack * 2f else attack }
        repeat(5) { v += if (it == 0) release * 2f else release }
        repeat(5) { v += 0f }
        return Codec.floatArray(v.size, v.toFloatArray())
    }

    /** Dynamic EQ presets: each slider is one band's maximum cut or boost (0 = off). */
    fun dynamicEq(s: Settings): ByteArray {
        // enabled, type, Hz, Q, threshold, range, ratio, attack, release, mode (0 above, 1 below)
        fun band(amount: Int, type: Int, hz: Float, q: Float, thr: Float, sign: Float, mode: Int) = floatArrayOf(
            if (amount > 0) 1f else 0f, type.toFloat(), hz, q, thr, sign * amount, 3f, 5f, 150f, mode.toFloat(),
        )
        val bands = listOf(
            band(s.int("deq.boom"), 0, 120f, 1.2f, -24f, -1f, 0),
            band(s.int("deq.harsh"), 0, 3500f, 1.5f, -30f, -1f, 0),
            band(s.int("deq.quietBass"), 1, 100f, 0.7f, -40f, 1f, 1),
            band(s.int("deq.quietAir"), 2, 8000f, 0.7f, -45f, 1f, 1),
        )
        return Codec.floatArray(bands.size, bands.flatMap { it.asList() }.toFloatArray())
    }
}
