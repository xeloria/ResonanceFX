// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Params
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.model.Composite
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class CompositeTest {
    private fun floats(payload: ByteArray): Pair<Int, FloatArray> {
        val b = ByteBuffer.wrap(payload).order(ByteOrder.nativeOrder())
        val count = b.getInt()
        return count to FloatArray((payload.size - 4) / 4) { b.getFloat() }
    }

    @Test fun bandsRoundTripAndClamp() {
        val bands = Composite.parseBands("0,1000,3,1;4,5,0,0.7;9,30000,40,50;garbage;1,100,-2.5")
        assertEquals(3, bands.size)
        assertEquals(Composite.Band(4, 20f, 0f, 0.7f), bands[1])
        assertEquals(Composite.Band(6, 20000f, 24f, 10f), bands[2])
        assertEquals(bands, Composite.parseBands(Composite.formatBands(bands)))
    }

    @Test fun graphicEqLayoutsMatchTheEngine() {
        assertEquals(15, Composite.graphicEqBands(1).size)
        assertEquals(31, Composite.graphicEqBands(2).size)
        assertEquals(31, Composite.parseGains("1;2;3", 31).size)
        assertEquals(12f, Composite.parseGains("40", 15)[0])
    }

    @Test fun multibandPayloadHasTheEngineLayout() {
        val (count, f) = floats(Composite.multiband(Settings.DEFAULT))
        assertEquals(33, count)
        assertEquals(Params.DDC_PAYLOAD_BYTES, 4 + f.size * 4)
        assertEquals(3f, f[0])               // bands
        assertEquals(1f, f[2])               // mix
        assertEquals(1f, f[3])               // auto makeup
        assertTrue(f[4] < f[5] && f[5] < f[6] && f[6] < f[7])  // crossovers ascending
        assertEquals(-18f, f[9])             // threshold of band 2
        assertEquals(2.5f, f[14])            // ratio of band 2
        val five = floats(Composite.multiband(Settings.DEFAULT.with("mbc.bands", 2))).second
        assertEquals(5f, five[0])
    }

    @Test fun dynamicEqDisablesBandsAtZero() {
        val (count, f) = floats(Composite.dynamicEq(Settings.DEFAULT.with("deq.boom", 0).with("deq.quietAir", 6)))
        assertEquals(4, count)
        assertEquals(0f, f[0])          // boom off
        assertEquals(1f, f[30])         // quiet air on
        assertEquals(6f, f[35])         // boosts by 6 dB
        assertEquals(1f, f[39])         // below threshold
    }

    @Test fun compositeSectionsCoverTheirControls() {
        listOf("peq.bands", "peq.preamp", "geq.layout", "geq.gains", "mbc.ratio", "deq.harsh").forEach {
            assertTrue(it, it in Catalog.composites)
        }
    }
}
