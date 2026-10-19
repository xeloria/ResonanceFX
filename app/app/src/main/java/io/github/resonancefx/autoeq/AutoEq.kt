// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.autoeq

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/** One AutoEq parametric filter. */
data class PeqFilter(val type: Int, val frequency: Float, val gainDb: Float, val q: Float) {
    companion object {
        const val PEAKING = 0
        const val LOW_SHELF = 1
        const val HIGH_SHELF = 2
        // Only user EQ bands use these; AutoEq profiles are peaks and shelves.
        const val LOW_PASS = 3
        const val HIGH_PASS = 4
        const val BAND_PASS = 5
        const val NOTCH = 6
    }
}

/** A headphone measured by one source, with AutoEq's correction for it. */
data class AutoEqProfile(
    val name: String,
    val source: String,
    val form: String,
    val preampDb: Float,
    val filters: List<PeqFilter>,
    /** Measurement rig within the source, e.g. "GRAS 43AG-7 in-ear"; empty if the source has one. */
    val rig: String = "",
) {
    /**
     * Stable key stored in settings. A model can be measured by several sources, and a source can
     * measure it on several rigs, so all three are part of the key.
     */
    val id: String get() = if (rig.isEmpty()) "$source/$name" else "$source/$rig/$name"

    /** "crinacle · 711": the rig without the form factor, which is shown separately. */
    val sourceLabel: String
        get() {
            val label = rig.replace(form, "").replace(Regex("\\s+"), " ").trim(' ', '/')
            return if (label.isEmpty()) source else "$source · $label"
        }

    /** Filters and preamp scaled to [strength] (0..1): 0 is flat, 1 is the full correction. */
    fun scaled(strength: Float): Pair<Float, List<PeqFilter>> {
        val s = strength.coerceIn(0f, 1f)
        val filters = filters.map { it.copy(gainDb = it.gainDb * s) }
        // Headroom for the largest boost of the scaled response, so the result never clips.
        val peak = (Response.curve(0f, filters).maxOrNull() ?: 0f).coerceAtLeast(0f)
        return -peak to filters
    }
}

/**
 * The bundled AutoEq database (https://github.com/jaakkopasanen/AutoEq, MIT): thousands of
 * headphones measured by oratory1990, crinacle, Rtings and others. Shipped inside the APK so it
 * works offline and the app needs no network access.
 */
class AutoEqDatabase private constructor(val commit: String, val profiles: List<AutoEqProfile>) {

    private val byId = profiles.associateBy { it.id }
    val modelCount = profiles.map { it.name.lowercase(Locale.ROOT) }.toSet().size

    /**
     * Looks a profile up by id. Ids saved before rigs were part of the key ("source/name") fall
     * back to that source's first measurement of the model.
     */
    fun find(id: String): AutoEqProfile? = byId[id] ?: id.indexOf('/').takeIf { it > 0 }?.let { slash ->
        val source = id.substring(0, slash)
        val name = id.substring(slash + 1)
        profiles.firstOrNull { it.source == source && it.name == name }
    }

    /** All profiles whose name contains every word of [query], best sources first. */
    fun search(query: String, limit: Int = 300): List<AutoEqProfile> {
        val words = normalize(query).split(' ').filter { it.isNotEmpty() }
        if (words.isEmpty()) return profiles.take(limit)
        return profiles.asSequence()
            .filter { p -> normalize(p.name).let { n -> words.all { it in n } } }
            .take(limit)
            .toList()
    }

    /**
     * Profiles matching a connected device's name, e.g. "WH-1000XM4" or "Galaxy Buds2 Pro".
     * Bluetooth names often omit the brand or use different punctuation, so models are matched
     * on their significant tokens rather than the whole string.
     */
    fun suggestionsFor(deviceName: String, limit: Int = 8): List<AutoEqProfile> {
        val device = tokens(deviceName)
        if (device.isEmpty()) return emptyList()
        return profiles.asSequence()
            .map { p -> p to score(device, tokens(p.name)) }
            .filter { it.second >= 0.6 }
            .sortedWith(compareByDescending<Pair<AutoEqProfile, Double>> { it.second })
            .map { it.first }
            .distinctBy { it.name.lowercase(Locale.ROOT) }
            .take(limit)
            .toList()
    }

    companion object {
        @Volatile private var instance: AutoEqDatabase? = null

        /** Loads (once) from assets. ~9k profiles; call off the main thread. */
        fun get(context: Context): AutoEqDatabase = instance ?: synchronized(this) {
            instance ?: context.assets.open("autoeq/profiles.json").use {
                parse(it.readBytes().decodeToString())
            }.also { instance = it }
        }

        fun parse(json: String): AutoEqDatabase {
            val root = JSONObject(json)
            val sources = root.getJSONArray("sources").strings()
            val forms = root.getJSONArray("forms").strings()
            val list = root.getJSONArray("profiles")
            val profiles = ArrayList<AutoEqProfile>(list.length())
            for (i in 0 until list.length()) {
                val p = list.getJSONArray(i)
                val f = p.getJSONArray(4)
                profiles += AutoEqProfile(
                    name = p.getString(0),
                    source = sources[p.getInt(1)],
                    form = forms[p.getInt(2)],
                    preampDb = p.getDouble(3).toFloat(),
                    filters = List(f.length()) { j ->
                        val x = f.getJSONArray(j)
                        PeqFilter(x.getInt(0), x.getDouble(1).toFloat(), x.getDouble(2).toFloat(), x.getDouble(3).toFloat())
                    },
                    rig = p.optString(5),
                )
            }
            return AutoEqDatabase(root.optString("commit"), profiles)
        }

        private fun JSONArray.strings() = List(length()) { getString(it) }

        private fun normalize(s: String) =
            s.lowercase(Locale.ROOT).replace(Regex("[^a-z0-9]+"), " ").trim()

        // Brand words and filler that say little about which model it is.
        private val stopWords = setOf("the", "and", "with", "headphones", "headphone", "earphones", "earbuds", "wireless", "true", "le", "bt", "hd")

        // Split "buds2" / "1000xm4" into "buds 2" / "1000 xm 4" so Bluetooth names and model names
        // with different spacing still share tokens.
        private val letterDigitBoundary = Regex("(?<=[a-z])(?=[0-9])|(?<=[0-9])(?=[a-z])")

        private fun tokens(s: String) =
            normalize(s).replace(letterDigitBoundary, " ")
                .split(' ').filter { it.isNotEmpty() && it !in stopWords }.toSet()

        /** Share of the device's tokens found in the model name, favouring exact model length. */
        private fun score(device: Set<String>, model: Set<String>): Double {
            if (model.isEmpty()) return 0.0
            val hits = device.count { it in model }
            if (hits == 0) return 0.0
            val recall = hits.toDouble() / device.size
            val precision = hits.toDouble() / model.size
            return 0.7 * recall + 0.3 * precision
        }
    }
}

/** Magnitude response of a filter set, matching the engine's RBJ biquads exactly. */
object Response {
    /** Log-spaced frequencies from 20 Hz to 20 kHz. */
    val frequencies: FloatArray = FloatArray(128) { (20.0 * 1000.0.pow(it / 127.0)).toFloat() }

    fun curve(preampDb: Float, filters: List<PeqFilter>, rate: Double = 48000.0): List<Float> =
        frequencies.map { at(preampDb, filters, it.toDouble(), rate) }

    /** Total gain in dB at one frequency. */
    fun at(preampDb: Float, filters: List<PeqFilter>, hz: Double, rate: Double = 48000.0): Float =
        preampDb + filters.sumOf { gainDb(it, hz, rate) }.toFloat()

    private fun gainDb(f: PeqFilter, hz: Double, fs: Double): Double {
        if (f.frequency >= fs * 0.4999) return 0.0
        val a = 10.0.pow(f.gainDb / 40.0)
        val w0 = 2 * PI * f.frequency / fs
        val cw = cos(w0)
        val alpha = sin(w0) / (2 * f.q)
        val sa = 2 * sqrt(a) * alpha
        val (b, d) = when (f.type) {
            PeqFilter.LOW_SHELF -> doubleArrayOf(
                a * ((a + 1) - (a - 1) * cw + sa), 2 * a * ((a - 1) - (a + 1) * cw), a * ((a + 1) - (a - 1) * cw - sa),
            ) to doubleArrayOf((a + 1) + (a - 1) * cw + sa, -2 * ((a - 1) + (a + 1) * cw), (a + 1) + (a - 1) * cw - sa)
            PeqFilter.HIGH_SHELF -> doubleArrayOf(
                a * ((a + 1) + (a - 1) * cw + sa), -2 * a * ((a - 1) + (a + 1) * cw), a * ((a + 1) + (a - 1) * cw - sa),
            ) to doubleArrayOf((a + 1) - (a - 1) * cw + sa, 2 * ((a - 1) - (a + 1) * cw), (a + 1) - (a - 1) * cw - sa)
            PeqFilter.LOW_PASS -> doubleArrayOf((1 - cw) / 2, 1 - cw, (1 - cw) / 2) to doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
            PeqFilter.HIGH_PASS -> doubleArrayOf((1 + cw) / 2, -(1 + cw), (1 + cw) / 2) to doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
            PeqFilter.BAND_PASS -> doubleArrayOf(alpha, 0.0, -alpha) to doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
            PeqFilter.NOTCH -> doubleArrayOf(1.0, -2 * cw, 1.0) to doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
            else -> doubleArrayOf(1 + alpha * a, -2 * cw, 1 - alpha * a) to doubleArrayOf(1 + alpha / a, -2 * cw, 1 - alpha / a)
        }
        // |H(e^jw)|^2 evaluated directly from the biquad polynomials.
        val w = 2 * PI * hz / fs
        fun mag2(c: DoubleArray): Double {
            val re = c[0] + c[1] * cos(w) + c[2] * cos(2 * w)
            val im = -(c[1] * sin(w) + c[2] * sin(2 * w))
            return re * re + im * im
        }
        return 10 * log10(max(mag2(b), 1e-30) / max(mag2(d), 1e-30))
    }
}
