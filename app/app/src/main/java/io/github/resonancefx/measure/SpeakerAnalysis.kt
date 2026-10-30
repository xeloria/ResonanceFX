// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.measure

import io.github.resonancefx.autoeq.PeqFilter
import io.github.resonancefx.autoeq.Response
import io.github.resonancefx.model.Composite
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.ln
import kotlin.math.log10
import kotlin.math.log2
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

/** A measurement that can't be used, with a message for the user. */
open class MeasureException(message: String) : Exception(message)

/** The microphone clipped: the sweep was too loud for it (or for the speaker). */
class OverloadException : MeasureException("The microphone overloaded. Turn the volume down a little and try again.")

/** In-place iterative radix-2 FFT (unscaled in both directions). */
internal object Fft {
    fun transform(re: DoubleArray, im: DoubleArray, inverse: Boolean = false) {
        val n = re.size
        require(n == im.size && n > 0 && n and (n - 1) == 0) { "FFT size must be a power of two" }
        var j = 0
        for (i in 1 until n) {
            var bit = n shr 1
            while (j and bit != 0) {
                j = j xor bit
                bit = bit shr 1
            }
            j = j xor bit
            if (i < j) {
                var t = re[i]; re[i] = re[j]; re[j] = t
                t = im[i]; im[i] = im[j]; im[j] = t
            }
        }
        var len = 2
        while (len <= n) {
            val half = len / 2
            val angle = (if (inverse) 2 else -2) * PI / len
            for (k in 0 until half) {
                val wr = cos(angle * k)
                val wi = sin(angle * k)
                var a = k
                while (a < n) {
                    val b = a + half
                    val tr = re[b] * wr - im[b] * wi
                    val ti = re[b] * wi + im[b] * wr
                    re[b] = re[a] - tr
                    im[b] = im[a] - ti
                    re[a] += tr
                    im[a] += ti
                    a += len
                }
            }
            len = len shl 1
        }
    }

    fun size(atLeast: Int): Int {
        var n = 1
        while (n < atLeast) n = n shl 1
        return n
    }
}

/**
 * Speaker measurement with an exponential sine sweep (Farina, AES 108th convention, 2000): the
 * recording is deconvolved into an impulse response, which is time-gated so late room echoes
 * and the sweep's harmonic distortion (which lands before the main peak) drop out, then turned
 * into a 1/6-octave smoothed magnitude response.
 */
object SpeakerAnalysis {
    const val RATE = 48000
    const val SWEEP_SECONDS = 4.0
    /** Silence recorded before the sweep (noise floor), and after it (latency, room decay). */
    const val LEAD_SECONDS = 0.5
    const val TAIL_SECONDS = 1.5
    private const val F1 = 20.0
    private const val F2 = 20000.0
    private const val WINDOW = 8192

    /** Excitation bands of the distortion measurement, matching the engine's measured protection. */
    val DISTORTION_BANDS = listOf(40.0 to 125.0, 125.0 to 250.0, 250.0 to 500.0, 500.0 to 1000.0, 1000.0 to 4000.0)

    val recordFrames: Int get() = ((LEAD_SECONDS + SWEEP_SECONDS + TAIL_SECONDS) * RATE).toInt()

    /** The sweep at -6 dBFS with 20 ms raised-cosine fades. */
    fun sweep(amplitude: Double = 0.5): FloatArray {
        val n = (SWEEP_SECONDS * RATE).toInt()
        val l = SWEEP_SECONDS / ln(F2 / F1)
        val fade = 0.02 * RATE
        return FloatArray(n) { i ->
            val t = i.toDouble() / RATE
            val edge = min(i, n - 1 - i) / fade
            val env = if (edge >= 1.0) 1.0 else 0.5 - 0.5 * cos(PI * edge)
            (amplitude * env * sin(2 * PI * F1 * l * (exp(t / l) - 1))).toFloat()
        }
    }

    /** One sweep's response on [Response.frequencies] (dB, not normalised). */
    class Capture(val responseDb: FloatArray, val snrDb: Double)

    /** The combined measurement: normalised to 0 dB in the midrange, with the usable range. */
    class Result(val responseDb: FloatArray, val lowHz: Float, val highHz: Float, val snrDb: Double)

    fun analyse(recording: FloatArray, sweep: FloatArray): Capture {
        val d = deconvolve(recording, sweep)
        // Gate: 2.5 ms before the peak to 100 ms after.
        val power = windowPower(d.ir, d.peak, (0.1 * RATE).toInt())
        return Capture(smooth(power, WINDOW), d.snrDb)
    }

    /**
     * Distortion of one sweep, per protection band ([DISTORTION_BANDS]): THD (2nd-4th harmonics)
     * as a fraction, or null where the harmonics were below the room's noise (i.e. clean).
     * An exponential sweep puts the k-th harmonic's impulse response L*ln(k) before the linear
     * one (Farina 2000), and at excitation frequency f its spectrum, read at k*f, relative to the
     * linear response at f is that harmonic's level.
     */
    fun analyseDistortion(recording: FloatArray, sweep: FloatArray): Array<Double?> {
        val d = deconvolve(recording, sweep)
        val l = SWEEP_SECONDS / ln(F2 / F1)
        val length = (0.08 * RATE).toInt()
        val linear = windowPower(d.ir, d.peak, length)
        val harmonics = (2..4).map { k -> windowPower(d.ir, d.peak - (l * ln(k.toDouble()) * RATE).roundToInt(), length) }
        // Between the 2nd harmonic and the linear response there is nothing but noise.
        val noise = windowPower(d.ir, d.peak - (0.25 * RATE).toInt(), length)
        val binHz = RATE.toDouble() / WINDOW
        return Array(DISTORTION_BANDS.size) { b ->
            val (lo, hi) = DISTORTION_BANDS[b]
            var fundamental = 0.0
            var distortion = 0.0
            var floor = 0.0
            for (bin in (lo / binHz).roundToInt()..(hi / binHz).roundToInt()) {
                fundamental += linear[bin]
                harmonics.forEachIndexed { i, h ->
                    val at = bin * (i + 2)
                    if (at < h.size) {
                        distortion += h[at]
                        floor += noise[at]
                    }
                }
            }
            // Harmonics within twice the noise can't be told apart from it.
            if (distortion < 2 * floor || fundamental <= 0.0) null else sqrt((distortion - floor) / fundamental)
        }
    }

    private class Deconvolved(val ir: DoubleArray, val peak: Int, val snrDb: Double)

    private fun deconvolve(recording: FloatArray, sweep: FloatArray): Deconvolved {
        if (recording.maxOf { abs(it) } > 0.98f) throw OverloadException()
        val lead = (LEAD_SECONDS * RATE).toInt()
        val noise = rms(recording, 0, (lead * 0.8).toInt())
        val signal = rms(recording, lead + RATE / 4, min(recording.size, lead + sweep.size))
        val snr = 20 * log10(max(signal, 1e-9) / max(noise, 1e-9))
        if (snr < 15) {
            throw MeasureException(
                "The sweep was hardly louder than the room (${snr.toInt()} dB). Turn the volume up, " +
                    "move closer or find a quieter spot.",
            )
        }

        // Regularised deconvolution: IR = IFFT(R conj(S) / (|S|^2 + eps)), band-limited to the sweep.
        val n = Fft.size(recording.size + sweep.size)
        val rRe = DoubleArray(n).also { a -> recording.forEachIndexed { i, v -> a[i] = v.toDouble() } }
        val rIm = DoubleArray(n)
        val sRe = DoubleArray(n).also { a -> sweep.forEachIndexed { i, v -> a[i] = v.toDouble() } }
        val sIm = DoubleArray(n)
        Fft.transform(rRe, rIm)
        Fft.transform(sRe, sIm)
        var maxPower = 0.0
        for (k in 0 until n) maxPower = max(maxPower, sRe[k] * sRe[k] + sIm[k] * sIm[k])
        val eps = 1e-5 * maxPower
        for (k in 0 until n) {
            val hz = min(k, n - k).toDouble() * RATE / n
            if (hz < F1 * 0.9 || hz > F2 * 1.05) {
                rRe[k] = 0.0; rIm[k] = 0.0
                continue
            }
            val p = sRe[k] * sRe[k] + sIm[k] * sIm[k] + eps
            val re = (rRe[k] * sRe[k] + rIm[k] * sIm[k]) / p
            val im = (rIm[k] * sRe[k] - rRe[k] * sIm[k]) / p
            rRe[k] = re
            rIm[k] = im
        }
        Fft.transform(rRe, rIm, inverse = true)
        val ir = rRe  // real part; the 1/n scale cancels in every ratio taken from it

        // The direct sound: the strongest peak at a plausible latency (up to the tail length).
        val maxDelay = min(n / 2, (TAIL_SECONDS * RATE).toInt() + lead)
        var peak = 0
        for (i in 0 until maxDelay) if (abs(ir[i]) > abs(ir[peak])) peak = i
        val far = rmsD(ir, min(n - 1, peak + RATE / 2), min(n, peak + RATE))
        if (abs(ir[peak]) < 30 * max(far, 1e-30)) {
            throw MeasureException("No clear sweep was picked up. Check the volume and that the microphone isn't covered.")
        }
        return Deconvolved(ir, peak, snr)
    }

    /**
     * Power spectrum ([WINDOW]-point FFT, first half) of the impulse response from 2.5 ms before
     * [centre] to [length] samples after it, with half-Hann edges. Indices wrap around: harmonic
     * responses sit before the start of the buffer.
     */
    private fun windowPower(ir: DoubleArray, centre: Int, length: Int): DoubleArray {
        val n = ir.size
        val pre = (0.0025 * RATE).toInt()
        val fadeOut = (0.025 * RATE).toInt()
        val total = min(pre + length, WINDOW)
        val re = DoubleArray(WINDOW)
        val im = DoubleArray(WINDOW)
        for (k in 0 until total) {
            val w = when {
                k < pre -> 0.5 - 0.5 * cos(PI * k / pre)
                total - k < fadeOut -> 0.5 - 0.5 * cos(PI * (total - k) / fadeOut)
                else -> 1.0
            }
            re[k] = ir[((centre - pre + k) % n + n) % n] * w
        }
        Fft.transform(re, im)
        return DoubleArray(WINDOW / 2) { re[it] * re[it] + im[it] * im[it] }
    }

    /** Averages the captures, normalises to the 300 Hz-3 kHz mean and finds the usable range. */
    fun combine(captures: List<Capture>): Result {
        val f = Response.frequencies
        val db = FloatArray(f.size) { i -> captures.map { it.responseDb[i] }.average().toFloat() }
        val mid = f.indices.filter { f[it] in 300f..3000f }.map { db[it] }.average().toFloat()
        for (i in db.indices) db[i] -= mid

        // The range on a third-octave view so single notches (room modes) don't end it early.
        val coarse = FloatArray(f.size) { i -> (max(0, i - 2)..min(f.lastIndex, i + 2)).map { db[it] }.average().toFloat() }
        var low = f.indexOfFirst { it >= 500f }
        while (low > 0 && coarse[low - 1] >= -10f) low--
        var high = f.indexOfFirst { it >= 3000f }
        while (high < f.lastIndex && coarse[high + 1] >= -12f) high++
        return Result(
            db,
            f[low].coerceIn(40f, 1000f),
            f[high].coerceIn(3000f, 16000f),
            captures.minOf { it.snrDb },
        )
    }

    private fun smooth(power: DoubleArray, fftSize: Int): FloatArray {
        val binHz = RATE.toDouble() / fftSize
        val sixth = 2.0.pow(1.0 / 12)  // +-1/12 octave = 1/6-octave window
        return FloatArray(Response.frequencies.size) { i ->
            val hz = Response.frequencies[i].toDouble()
            val lo = (hz / sixth / binHz).toInt().coerceIn(1, power.size - 1)
            val hi = (hz * sixth / binHz).toInt().coerceIn(lo, power.size - 1)
            var sum = 0.0
            for (k in lo..hi) sum += power[k]
            (10 * log10(max(sum / (hi - lo + 1), 1e-30))).toFloat()
        }
    }

    private fun rms(x: FloatArray, from: Int, to: Int): Double {
        if (to <= from) return 0.0
        var s = 0.0
        for (i in from until to) s += x[i].toDouble() * x[i]
        return sqrt(s / (to - from))
    }

    private fun rmsD(x: DoubleArray, from: Int, to: Int): Double {
        if (to <= from) return 0.0
        var s = 0.0
        for (i in from until to) s += x[i] * x[i]
        return sqrt(s / (to - from))
    }
}

/**
 * Fits a parametric correction to a measured speaker. Greedy peak placement on the largest
 * remaining error, each band then refined by a small local search, as AutoEq does for
 * headphones. Corrections stay gentle because phone microphones aren't calibrated: no more
 * than +6 dB of boost, and nothing outside the range the speaker can actually play.
 */
object CorrectionFitter {
    const val MAX_BANDS = 10
    const val MAX_BOOST = 6f
    const val MAX_CUT = -12f

    /** Loudspeakers sound most natural with a slight downward tilt (-0.5 dB/octave, 0 at 1 kHz). */
    fun targetDb(hz: Float): Float = (-0.5 * log2(hz / 1000.0)).toFloat()

    /**
     * Fits the correction for [result]. [targetDb] (on [Response.frequencies]) is what the speaker
     * should measure afterwards; null means the natural loudspeaker target, [targetDb].
     */
    fun fit(result: SpeakerAnalysis.Result, targetDb: FloatArray? = null): Composite.Measurement {
        val f = Response.frequencies
        val n = f.size
        val lowOct = log2(result.lowHz.toDouble())
        val highOct = log2(result.highHz.toDouble())
        // Full weight inside the usable range, fading out over a third of an octave.
        val inRange = FloatArray(n) { i ->
            val o = log2(f[i].toDouble())
            val outside = max(lowOct - o, o - highOct)
            (1.0 - outside * 3.0).coerceIn(0.0, 1.0).toFloat()
        }
        val desired = FloatArray(n) { i ->
            ((targetDb?.get(i) ?: targetDb(f[i])) - result.responseDb[i]).coerceIn(MAX_CUT, MAX_BOOST) * inRange[i]
        }
        // Outside the range the correction should stay flat, but matters less.
        val weight = FloatArray(n) { max(inRange[it], 0.25f) }

        fun curve(band: PeqFilter) = FloatArray(n) { Response.at(0f, listOf(band), f[it].toDouble()) }
        fun error(others: FloatArray, c: FloatArray): Double {
            var e = 0.0
            for (i in 0 until n) {
                val d = desired[i] - others[i] - c[i]
                e += weight[i] * d * d
            }
            return e
        }
        fun clampBand(b: PeqFilter) = b.copy(
            frequency = b.frequency.coerceIn(20f, 20000f),
            gainDb = b.gainDb.coerceIn(MAX_CUT, MAX_BOOST),
            q = b.q.coerceIn(0.4f, 6f),
        )

        val bands = ArrayList<PeqFilter>()
        val curves = ArrayList<FloatArray>()
        fun sumExcept(skip: Int) = FloatArray(n) { i -> curves.indices.filter { it != skip }.sumOf { curves[it][i].toDouble() }.toFloat() }

        /** Local search around [start] given the other bands' total. */
        fun refine(start: PeqFilter, others: FloatArray, gainSteps: FloatArray, qSteps: FloatArray, fSteps: FloatArray): Pair<PeqFilter, FloatArray> {
            var best = start
            var bestCurve = curve(start)
            var bestErr = error(others, bestCurve)
            for (gs in gainSteps) for (qs in qSteps) for (fs in fSteps) {
                val c = clampBand(PeqFilter(PeqFilter.PEAKING, best.frequency * fs, best.gainDb * gs, best.q * qs))
                val cc = curve(c)
                val e = error(others, cc)
                if (e < bestErr) {
                    best = c; bestCurve = cc; bestErr = e
                }
            }
            return best to bestCurve
        }

        repeat(MAX_BANDS) {
            val others = sumExcept(-1)
            var idx = -1
            for (i in 0 until n) {
                if (inRange[i] <= 0f) continue
                if (idx < 0 || abs(desired[i] - others[i]) > abs(desired[idx] - others[idx])) idx = i
            }
            if (idx < 0) return@repeat
            val g = desired[idx] - others[idx]
            if (abs(g) < 1f) return@repeat
            // Width from where the remaining error falls to half its peak.
            var a = idx
            while (a > 0 && (desired[a - 1] - others[a - 1]) * g > g * g / 2) a--
            var b = idx
            while (b < n - 1 && (desired[b + 1] - others[b + 1]) * g > g * g / 2) b++
            val bw = max(log2(f[b].toDouble() / f[a]), 0.2)
            val q = (sqrt(2.0.pow(bw)) / (2.0.pow(bw) - 1)).toFloat()
            val (band, c) = refine(
                clampBand(PeqFilter(PeqFilter.PEAKING, f[idx], g, q)), others,
                floatArrayOf(0.7f, 0.85f, 1f, 1.15f), floatArrayOf(0.6f, 0.8f, 1f, 1.25f, 1.6f),
                floatArrayOf(0.944f, 1f, 1.059f),
            )
            bands += band
            curves += c
        }
        // Polish every band against all the others.
        repeat(2) {
            for (k in bands.indices) {
                val (band, c) = refine(
                    bands[k], sumExcept(k),
                    floatArrayOf(0.8f, 0.9f, 1f, 1.1f, 1.2f), floatArrayOf(0.85f, 1f, 1.18f), floatArrayOf(0.971f, 1f, 1.03f),
                )
                bands[k] = band
                curves[k] = c
            }
        }
        val kept = bands.indices.filter { abs(bands[it].gainDb) >= 0.3f }
        val total = FloatArray(n) { i -> kept.sumOf { curves[it][i].toDouble() }.toFloat() }
        val preamp = -max(0f, total.maxOrNull() ?: 0f)
        return Composite.Measurement(
            preampDb = (preamp * 10).toInt() / 10f,
            bands = kept.map { bands[it] }.sortedBy { it.frequency }.map { Composite.Band(0, it.frequency, it.gainDb, it.q) },
            responseDb = result.responseDb.toList(),
            lowHz = result.lowHz,
            highHz = result.highHz,
        )
    }

    /**
     * Refits a stored measurement: to the natural target ([reference] null), or so that this
     * speaker sounds like [reference], another measured speaker. Matching only covers the range
     * both can play.
     */
    fun refit(m: Composite.Measurement, reference: Composite.Measurement?): Composite.Measurement {
        val low = if (reference == null) m.lowHz else maxOf(m.lowHz, reference.lowHz)
        val high = if (reference == null) m.highHz else minOf(m.highHz, reference.highHz)
        val result = SpeakerAnalysis.Result(m.responseDb.toFloatArray(), low, maxOf(high, low * 2), 0.0)
        return fit(result, reference?.responseDb?.toFloatArray())
            .copy(lowHz = m.lowHz, highHz = m.highHz, distortion = m.distortion)
    }

    /** The correction's response on [Response.frequencies], without the preamp. */
    fun correctionDb(m: Composite.Measurement): FloatArray =
        Response.curve(0f, m.bands.map { PeqFilter(it.type, it.frequency, it.gainDb, it.q) }).toFloatArray()
}
