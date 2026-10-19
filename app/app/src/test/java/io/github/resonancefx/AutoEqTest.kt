// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.autoeq.AutoEqDatabase
import io.github.resonancefx.autoeq.PeqFilter
import io.github.resonancefx.autoeq.Response
import io.github.resonancefx.driver.Codec
import io.github.resonancefx.driver.Params
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.abs

class AutoEqTest {
    companion object {
        lateinit var db: AutoEqDatabase

        @BeforeClass @JvmStatic fun load() {
            // Unit tests run from the module directory; read the real bundled asset.
            db = AutoEqDatabase.parse(File("src/main/assets/autoeq/profiles.json").readText())
        }
    }

    private fun hd650() = db.profiles.first { it.name == "Sennheiser HD 650" && it.source == "oratory1990" }

    @Test fun bundledDatabaseLoads() {
        assertEquals(8850, db.profiles.size)
        assertTrue(db.modelCount > 6000)
        val hd650 = hd650()
        assertEquals(10, hd650.filters.size)
        assertEquals(-6.1f, hd650.preampDb, 0.001f)
        assertEquals(PeqFilter.LOW_SHELF, hd650.filters[0].type)
        assertEquals("over-ear", hd650.form)
        assertEquals(hd650, db.find("oratory1990/over-ear/Sennheiser HD 650"))
        assertEquals("oratory1990", hd650.sourceLabel)
    }

    @Test fun profileIdsAreUnique() {
        // The picker uses ids as list keys and settings store them: duplicates crash and collide.
        val ids = db.profiles.map { it.id }
        assertEquals(ids.size, ids.toSet().size)
        assertTrue(db.profiles.all { db.find(it.id) === it })
    }

    @Test fun legacyIdsWithoutRigStillResolve() {
        // 1.1.0 pre-release stored "source/name"; the same model on that source must still load.
        val p = db.find("crinacle/1Custom SA05")
        assertTrue(p != null && p.name == "1Custom SA05" && p.source == "crinacle")
        assertEquals(null, db.find("crinacle/Does Not Exist"))
    }

    @Test fun searchMatchesEveryWord() {
        val r = db.search("sennheiser 650")
        assertTrue(r.isNotEmpty())
        assertTrue(r.all { it.name.contains("Sennheiser", true) && it.name.contains("650") })
        // Best source first for each model (oratory1990 is preferred).
        assertEquals("oratory1990", r.first { it.name == "Sennheiser HD 650" }.source)
    }

    @Test fun suggestsModelsForBluetoothNames() {
        // Bluetooth names often lack the brand and use different punctuation.
        assertTrue(db.suggestionsFor("WH-1000XM4").any { it.name.contains("WH-1000XM4", true) })
        assertTrue(db.suggestionsFor("Galaxy Buds2 Pro").any { it.name.contains("Buds2 Pro", true) })
        assertTrue(db.suggestionsFor("Zqxv Nonexistent").isEmpty())
    }

    @Test fun responseMatchesTheEngineFilters() {
        // Same filter as the engine's host test: +6 dB peak at 1 kHz, Q 1, with -3 dB preamp.
        val peak = listOf(PeqFilter(PeqFilter.PEAKING, 1000f, 6f, 1f))
        assertEquals(3f, Response.at(-3f, peak, 1000.0), 0.01f)
        assertEquals(-3f, Response.at(-3f, peak, 20.0), 0.05f)
        // Shelves reach their full gain far from the corner and half of it at the corner.
        val low = listOf(PeqFilter(PeqFilter.LOW_SHELF, 100f, 6f, 0.707f))
        assertEquals(6f, Response.at(0f, low, 20.0), 0.3f)
        assertEquals(3f, Response.at(0f, low, 100.0), 0.2f)
        assertEquals(0f, Response.at(0f, low, 10000.0), 0.05f)
        // A filter above Nyquist is skipped exactly like the engine does.
        assertEquals(0f, Response.at(0f, listOf(PeqFilter(0, 30000f, 6f, 1f)), 1000.0), 0f)
    }

    @Test fun strengthScalesGainsAndKeepsHeadroom() {
        val p = hd650()
        val (flatPreamp, flat) = p.scaled(0f)
        assertEquals(0f, flatPreamp, 0.001f)
        assertTrue(Response.curve(flatPreamp, flat).all { abs(it) < 0.001f })

        val (preamp, full) = p.scaled(1f)
        assertTrue(preamp < 0f)
        // With the computed preamp the corrected response never boosts above 0 dB.
        assertTrue(Response.curve(preamp, full).maxOrNull()!! <= 0.01f)
        assertEquals(p.filters[1].gainDb * 0.5f, p.scaled(0.5f).second[1].gainDb, 0.0001f)
    }

    @Test fun codecMatchesEngineLayout() {
        val bytes = Codec.parametricEq(-4.5f, listOf(floatArrayOf(0f, 1000f, 6f, 1f), floatArrayOf(2f, 8000f, -2f, 0.7f)))
        assertEquals(Params.DDC_PAYLOAD_BYTES, bytes.size)
        val b = ByteBuffer.wrap(bytes).order(ByteOrder.nativeOrder())
        assertEquals(2, b.getInt(0))
        assertEquals(-4.5f, b.getFloat(4))
        assertEquals(1000f, b.getFloat(12))
        assertEquals(2f, b.getFloat(24))
        assertEquals(0.7f, b.getFloat(36))
    }
}
