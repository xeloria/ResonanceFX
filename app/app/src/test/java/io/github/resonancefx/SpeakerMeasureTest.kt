// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.autoeq.Response
import io.github.resonancefx.measure.CorrectionFitter
import io.github.resonancefx.measure.MeasureException
import io.github.resonancefx.measure.SpeakerAnalysis
import io.github.resonancefx.model.Composite
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.random.Random

class SpeakerMeasureTest {
    private val rate = SpeakerAnalysis.RATE.toDouble()

    /** RBJ biquad applied in place: type 0 peak, 4 high-pass. */
    private fun biquad(x: DoubleArray, type: Int, f0: Double, gainDb: Double, q: Double) {
        val a = 10.0.pow(gainDb / 40)
        val w = 2 * PI * f0 / rate
        val alpha = sin(w) / (2 * q)
        val cw = cos(w)
        val (b, d) = if (type == 4) {
            doubleArrayOf((1 + cw) / 2, -(1 + cw), (1 + cw) / 2) to doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
        } else if (type == 3) {
            doubleArrayOf((1 - cw) / 2, 1 - cw, (1 - cw) / 2) to doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
        } else {
            doubleArrayOf(1 + alpha * a, -2 * cw, 1 - alpha * a) to doubleArrayOf(1 + alpha / a, -2 * cw, 1 - alpha / a)
        }
        var x1 = 0.0; var x2 = 0.0; var y1 = 0.0; var y2 = 0.0
        for (i in x.indices) {
            val y = (b[0] * x[i] + b[1] * x1 + b[2] * x2 - d[1] * y1 - d[2] * y2) / d[0]
            x2 = x1; x1 = x[i]; y2 = y1; y1 = y
            x[i] = y
        }
    }

    /** What the microphone hears: the sweep through a 300 Hz high-pass and a +6 dB peak at 2 kHz. */
    private fun recording(level: Double = 0.3, noise: Double = 0.0005, seed: Int = 1): FloatArray {
        val sweep = SpeakerAnalysis.sweep()
        val total = SpeakerAnalysis.recordFrames
        val start = (SpeakerAnalysis.LEAD_SECONDS * rate).toInt() + (0.03 * rate).toInt()  // 30 ms latency
        val x = DoubleArray(total)
        for (i in sweep.indices) if (start + i < total) x[start + i] = sweep[i] * level
        biquad(x, 4, 300.0, 0.0, 0.7071)
        biquad(x, 0, 2000.0, 6.0, 1.0)
        val rnd = Random(seed)
        return FloatArray(total) { (x[it] + noise * (rnd.nextDouble() * 2 - 1) * sqrt(3.0)).toFloat() }
    }

    private fun at(db: FloatArray, hz: Float): Float = db[Response.frequencies.indexOfFirst { it >= hz }]

    @Test fun measuresAKnownSpeaker() {
        val sweep = SpeakerAnalysis.sweep()
        val result = SpeakerAnalysis.combine(listOf(SpeakerAnalysis.analyse(recording(), sweep)))
        // Relative to 1 kHz (the normalisation is the 300 Hz-3 kHz mean): +6 dB peak at 2 kHz
        // less its ~+1.6 dB skirt at 1 kHz, flat at 8 kHz, about -12 dB an octave below 300 Hz.
        val ref = at(result.responseDb, 1000f)
        assertEquals(4.4f, at(result.responseDb, 2000f) - ref, 0.8f)
        assertEquals(-1.6f, at(result.responseDb, 8000f) - ref, 0.8f)
        assertTrue(at(result.responseDb, 150f) - ref < -9f)
        // The usable range starts where the high-pass has rolled off by 10 dB.
        assertTrue("low ${result.lowHz}", result.lowHz in 120f..250f)
        assertEquals(16000f, result.highHz, 1f)
        assertTrue(result.snrDb > 40)
    }

    @Test fun correctionFlattensTheMeasurementWithinItsLimits() {
        val sweep = SpeakerAnalysis.sweep()
        val result = SpeakerAnalysis.combine(listOf(SpeakerAnalysis.analyse(recording(), sweep)))
        val m = CorrectionFitter.fit(result)
        assertTrue(m.bands.size in 1..CorrectionFitter.MAX_BANDS)
        assertTrue(m.bands.all { it.gainDb in CorrectionFitter.MAX_CUT..CorrectionFitter.MAX_BOOST })
        // Something near 2 kHz cuts the peak.
        assertTrue(m.bands.any { it.frequency in 1500f..2700f && it.gainDb < -3f })
        val correction = CorrectionFitter.correctionDb(m)
        val f = Response.frequencies
        var worst = 0f
        for (i in f.indices) {
            if (f[i] < result.lowHz * 1.3f || f[i] > result.highHz / 1.3f) continue
            worst = maxOf(worst, abs(result.responseDb[i] + correction[i] - CorrectionFitter.targetDb(f[i])))
        }
        assertTrue("worst in-range error $worst dB", worst < 1.5f)
        // Nothing is boosted below the range the speaker can play.
        assertTrue(correction[f.indexOfFirst { it >= 60f }] < 1f)
        assertTrue(m.preampDb <= 0f)

        val stored = Composite.parseMeasurement(Composite.formatMeasurement(m))
        assertNotNull(stored)
        assertEquals(m.bands.size, stored!!.bands.size)
        assertEquals(m.responseDb.size, stored.responseDb.size)
    }

    @Test fun matchesOneSpeakerToAnother() {
        val f = Response.frequencies
        val peak = listOf(io.github.resonancefx.autoeq.PeqFilter(0, 2000f, 6f, 1f))
        // A: flat. B: a +6 dB peak at 2 kHz. Both play 100 Hz - 16 kHz.
        val a = Composite.Measurement(0f, emptyList(), List(f.size) { 0f }, 100f, 16000f)
        val b = Composite.Measurement(0f, emptyList(), f.map { Response.at(0f, peak, it.toDouble()) }, 100f, 16000f)
        for ((from, to) in listOf(b to a, a to b)) {
            val m = CorrectionFitter.refit(from, to)
            val c = CorrectionFitter.correctionDb(m)
            var worst = 0f
            for (i in f.indices) {
                if (f[i] < 150f || f[i] > 12000f) continue
                worst = maxOf(worst, abs(from.responseDb[i] + c[i] - to.responseDb[i]))
            }
            assertTrue("matched curve off by $worst dB", worst < 1.0f)
        }
        // Back to the natural target: B's correction cuts the peak.
        val natural = CorrectionFitter.correctionDb(CorrectionFitter.refit(b, null))
        assertTrue(natural[f.indexOfFirst { it >= 2000f }] < -4f)
    }

    /**
     * A speaker whose bass (below ~250 Hz) saturates (tanh) as the level rises while the rest
     * stays linear, recorded at sweep level [levelDb].
     */
    private fun distortingRecording(levelDb: Double): Pair<FloatArray, FloatArray> {
        val sweep = SpeakerAnalysis.sweep(10.0.pow(levelDb / 20))
        val total = SpeakerAnalysis.recordFrames
        val start = (SpeakerAnalysis.LEAD_SECONDS * rate).toInt() + (0.02 * rate).toInt()
        val x = DoubleArray(total)
        for (i in sweep.indices) if (start + i < total) x[start + i] = sweep[i].toDouble()
        val low = x.copyOf()
        biquad(low, 3, 250.0, 0.0, 0.7071)
        biquad(low, 3, 250.0, 0.0, 0.7071)
        val g = 2.5
        val rnd = Random(3)
        val out = FloatArray(total) { i ->
            val high = x[i] - low[i]
            (0.3 * (kotlin.math.tanh(g * low[i]) / g + high) + 0.0002 * (rnd.nextDouble() * 2 - 1)).toFloat()
        }
        return out to sweep
    }

    @Test fun findsWhereTheBassStartsToDistort() {
        val quiet = distortingRecording(-12.0).let { (r, s) -> SpeakerAnalysis.analyseDistortion(r, s) }
        val loud = distortingRecording(0.0).let { (r, s) -> SpeakerAnalysis.analyseDistortion(r, s) }
        // Deep bass: clean (< 5 %) quietly, clearly distorted at full scale.
        assertTrue("quiet bass THD ${quiet[0]}", (quiet[0] ?: 0.0) < 0.05)
        assertTrue("loud bass THD ${loud[0]}", (loud[0] ?: 0.0) > 0.10)
        // Upper mids (1-4 kHz) never pass through the saturating part.
        assertTrue("loud 1-4 kHz THD ${loud[4]}", (loud[4] ?: 0.0) < 0.02)
    }

    @Test fun distortionResultsRoundTripAndBecomeCeilings() {
        val d = Composite.Distortion(-20f, listOf(-9f, -6f, null, null, null))
        val m = Composite.Measurement(0f, emptyList(), listOf(0f, 1f), 100f, 16000f, d)
        val back = Composite.parseMeasurement(Composite.formatMeasurement(m))
        assertEquals(d, back!!.distortion)
        // Engine ceilings: the clean level plus the volume Android applied first; 0 = no limit.
        assertEquals(listOf(-29f, -26f, 0f, 0f, 0f), d.ceilings().toList())
        // Measurements saved before the distortion test existed still load.
        assertNotNull(Composite.parseMeasurement("0.00;100;16000||0.0,1.0"))
    }

    @Test(expected = MeasureException::class) fun rejectsANoisyRoom() {
        SpeakerAnalysis.analyse(recording(level = 0.01, noise = 0.02), SpeakerAnalysis.sweep())
    }

    @Test(expected = MeasureException::class) fun rejectsAClippedRecording() {
        SpeakerAnalysis.analyse(recording(level = 3.0), SpeakerAnalysis.sweep())
    }

    @Test fun malformedMeasurementsAreIgnored() {
        assertEquals(null, Composite.parseMeasurement(""))
        assertEquals(null, Composite.parseMeasurement("x|y"))
        assertEquals(null, Composite.parseMeasurement("a;b;c|0,1000,1,1|1,2"))
    }
}
