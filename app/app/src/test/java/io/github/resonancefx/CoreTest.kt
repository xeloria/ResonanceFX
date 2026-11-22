// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Codec
import io.github.resonancefx.driver.Params
import io.github.resonancefx.engine.Applier
import io.github.resonancefx.engine.Assets
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.model.Choice
import io.github.resonancefx.model.Slider
import io.github.resonancefx.model.Tables
import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin

class CatalogTest {
    @Test fun keysAreUnique() {
        val keys = Catalog.sections.flatMap { s -> listOfNotNull(s.enableKey) + s.controls.map { it.key } } + Catalog.KEY_MASTER
        assertEquals(keys.size, keys.toSet().size)
    }

    @Test fun everySliderDefaultIsInRange() {
        // Value formats need Android resources; they are exercised on the device.
        Catalog.sections.flatMap { it.controls }.filterIsInstance<Slider>().forEach { s ->
            assertTrue("${s.key} default out of range", s.default in s.range)
        }
    }

    @Test fun everyChoiceDefaultIsValid() {
        Catalog.sections.flatMap { it.controls }.filterIsInstance<Choice>().forEach { c ->
            assertTrue(c.key, c.default in c.options.indices)
        }
        assertEquals(Tables.DYNAMIC_SYSTEM_DEVICES[4].second.toList(), listOf(100, 5600, 40, 80, 50, 50))
    }

    @Test fun tablesMatchOriginalApp() {
        assertEquals(22, Tables.OUTPUT_VOLUME_VALUES.size)
        assertEquals(100, Tables.OUTPUT_VOLUME_VALUES[11]) // default output gain = 0 dB
        assertEquals("0.0", Tables.OUTPUT_VOLUME_DB[11])
        Tables.EQ_PRESETS.forEach { (_, v) -> assertEquals("EQ preset", 10, v.split(';').size) }
    }
}

class CodecTest {
    private fun ints(bytes: ByteArray): List<Int> {
        val b = ByteBuffer.wrap(bytes).order(ByteOrder.nativeOrder())
        return List(bytes.size / 4) { b.getInt(it * 4) }
    }

    @Test fun eqBandIsIndexAndCentiDb() {
        assertEquals(listOf(3, -450), ints(Codec.eqBand(3, -4.5f)))
    }

    @Test fun ddcPayloadIs1024BytesWithPairCount() {
        val payload = Codec.ddc(FloatArray(20) { it.toFloat() })
        assertEquals(Params.DDC_PAYLOAD_BYTES, payload.size)
        assertEquals(10, ints(payload)[0])
        assertEquals(19f, ByteBuffer.wrap(payload).order(ByteOrder.nativeOrder()).getFloat(4 + 19 * 4))
    }

    @Test fun kernelChunksMatchEngineFraming() {
        val data = ByteArray(8184 * 2 + 100) { it.toByte() }
        val chunks = Codec.kernelChunks(77, data)
        assertEquals(3, chunks.size)
        chunks.forEach { assertEquals(8192, it.size) }
        assertEquals(listOf(77, 2046), ints(chunks[0]).take(2))
        assertEquals(listOf(77, 25), ints(chunks[2]).take(2))
        // Payload bytes follow the 8-byte header unchanged.
        assertArrayEquals(data.copyOfRange(8184, 8184 + 16), chunks[1].copyOfRange(8, 24))
    }
}

class AssetsTest {
    @Test fun parsesVdc() {
        val coefs = Assets.parseVdc("SR_44100:1,2,3,4,5\r\nSR_48000:6,7,8,9,10\n")
        assertArrayEquals(floatArrayOf(1f, 2f, 3f, 4f, 5f, 6f, 7f, 8f, 9f, 10f), coefs, 0f)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsVdcWithMismatchedLists() {
        Assets.parseVdc("SR_44100:1,2,3\nSR_48000:1,2")
    }

    private fun wav(channels: Int, rate: Int, bits: Int, float: Boolean, frames: Int, extensible: Boolean = false, junk: Boolean = false): ByteArray {
        val bps = bits / 8
        val data = ByteBuffer.allocate(frames * channels * bps).order(ByteOrder.LITTLE_ENDIAN)
        for (i in 0 until frames * channels) {
            val v = 0.5 * sin(2 * PI * i / 32)
            when {
                float -> data.putFloat(v.toFloat())
                bits == 16 -> data.putShort((v * 32767).toInt().toShort())
                bits == 24 -> { val q = (v * 8388607).toInt(); data.put(q.toByte()); data.put((q shr 8).toByte()); data.put((q shr 16).toByte()) }
                else -> data.putInt((v * Int.MAX_VALUE).toInt())
            }
        }
        val out = ByteArrayOutputStream()
        fun le32(v: Int) = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()
        fun le16(v: Int) = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort(v.toShort()).array()
        val fmt = ByteArrayOutputStream().apply {
            write(le16(if (extensible) 0xFFFE else if (float) 3 else 1)); write(le16(channels)); write(le32(rate))
            write(le32(rate * channels * bps)); write(le16(channels * bps)); write(le16(bits))
            if (extensible) { write(le16(22)); write(le16(bits)); write(le32(0)); write(le16(if (float) 3 else 1)); write(ByteArray(14)) }
        }.toByteArray()
        val body = ByteArrayOutputStream().apply {
            write("WAVE".toByteArray())
            write("fmt ".toByteArray()); write(le32(fmt.size)); write(fmt)
            if (junk) { write("LIST".toByteArray()); write(le32(3)); write(byteArrayOf(1, 2, 3, 0)) }
            write("data".toByteArray()); write(le32(data.capacity())); write(data.array())
        }.toByteArray()
        out.write("RIFF".toByteArray()); out.write(le32(body.size)); out.write(body)
        return out.toByteArray()
    }

    @Test fun parsesAllWavEncodings() {
        for ((bits, float) in listOf(16 to false, 24 to false, 32 to false, 32 to true)) {
            val k = Assets.parseWav(wav(2, 48000, bits, float, 500))
            assertEquals(2, k.channels); assertEquals(500, k.frames)
            assertEquals("bits=$bits float=$float", 0.5 * sin(2 * PI * 8 / 32), k.samples[8].toDouble(), 1e-3)
        }
    }

    @Test fun parsesExtensibleWavWithExtraChunks() {
        val k = Assets.parseWav(wav(1, 44100, 24, false, 300, extensible = true, junk = true))
        assertEquals(1, k.channels); assertEquals(300, k.frames)
    }

    @Test fun resamplingPreservesDurationAndLevel() {
        val k = Assets.parseWav(wav(1, 44100, 32, true, 4410))
        val r = Assets.resample(k, 48000)
        assertEquals(48000, r.sampleRate)
        assertEquals(4800, r.frames)
        val peakIn = k.samples.maxOf { abs(it) }
        val peakOut = r.samples.drop(100).dropLast(100).maxOf { abs(it) }
        assertEquals(peakIn.toDouble(), peakOut.toDouble(), 0.03)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsNonWav() {
        Assets.parseWav(ByteArray(100))
    }
}

class SettingsTest {
    @Test fun roundTripsThroughJson() {
        val s = Settings.DEFAULT.with("bass.enable", true).with("bass.gain", 7).with("eq.bands", "1.0;2.0;3.0;4.0;5.0;6.0;7.0;8.0;9.0;10.0")
        val back = Settings.fromJson(JSONObject(s.toJson().toString()))
        assertEquals(s, back)
        assertTrue(back.bool("bass.enable"))
        assertEquals(7, back.int("bass.gain"))
    }

    @Test fun clampsImportedValuesAndIgnoresUnknownKeys() {
        val json = JSONObject().put("values", JSONObject().put("bass.gain", 999).put("cure.strength", -3).put("nope", 1))
        val s = Settings.fromJson(json)
        assertEquals(170, s.int("bass.gain"))
        assertEquals(0, s.int("cure.strength"))
    }

    @Test fun migratesV1BassGainIndexToDbX10() {
        val legacy = JSONObject().put("format", "resonancefx.settings.v1")
            .put("values", JSONObject().put("bass.gain", 6))
        assertEquals(130, Settings.fromJson(legacy).int("bass.gain")) // step 6 was 13.0 dB

        val legacyMin = JSONObject().put("format", "resonancefx.settings.v1")
            .put("values", JSONObject().put("bass.gain", 0))
        assertEquals(35, Settings.fromJson(legacyMin).int("bass.gain")) // step 0 was 3.5 dB

        val current = JSONObject().put("format", "resonancefx.settings.v2")
            .put("values", JSONObject().put("bass.gain", 6))
        assertEquals(6, Settings.fromJson(current).int("bass.gain")) // v2 passes through (0.6 dB)
    }

    @Test fun migratesV2MasterSlidersFromTableIndices() {
        val v2 = JSONObject().put("format", "resonancefx.settings.v2").put("values", JSONObject()
            .put(Catalog.KEY_MASTER_VOLUME, 11)   // step 11 was 0.0 dB
            .put(Catalog.KEY_MASTER_LIMIT, 5))    // step 5 was 0.0 dB
        val s = Settings.fromJson(v2)
        assertEquals(0, s.int(Catalog.KEY_MASTER_VOLUME))
        assertEquals(0, s.int(Catalog.KEY_MASTER_LIMIT))

        val quiet = JSONObject().put("format", "resonancefx.settings.v2").put("values", JSONObject()
            .put(Catalog.KEY_MASTER_VOLUME, 6)    // step 6 was -6.0 dB
            .put(Catalog.KEY_MASTER_LIMIT, 0))    // step 0 was -10.5 dB
        val q = Settings.fromJson(quiet)
        assertEquals(-12, q.int(Catalog.KEY_MASTER_VOLUME))
        assertEquals(-21, q.int(Catalog.KEY_MASTER_LIMIT))
    }

    @Test fun v2MasterLabelsSurviveTheMigration() {
        // Every old step must land within a quarter dB of the label it used to show, so an
        // existing profile never moves audibly when it is upgraded.
        Tables.OUTPUT_VOLUME_DB.forEachIndexed { i, label ->
            val half = Catalog.halfDb(Tables.OUTPUT_VOLUME_VALUES[i])
            assertTrue("volume ${Tables.OUTPUT_VOLUME_VALUES[i]} = ${half / 2f} dB, was $label dB",
                abs(half / 2f - label.toFloat()) <= 0.25f)
        }
        Tables.OUTPUT_DB.forEachIndexed { i, label ->
            val half = Catalog.halfDb(Tables.OUTPUT_DB_VALUES[i])
            assertTrue("limit ${Tables.OUTPUT_DB_VALUES[i]} = ${half / 2f} dB, was $label dB",
                abs(half / 2f - label.toFloat()) <= 0.25f)
        }
    }

    @Test fun migratesV1MasterSlidersAlongsideBassGain() {
        val v1 = JSONObject().put("values", JSONObject()
            .put(Catalog.KEY_MASTER_VOLUME, 4)    // step 4 was -10.5 dB
            .put(Catalog.KEY_MASTER_LIMIT, 1)     // step 1 was -6.0 dB
            .put("bass.gain", 6))                 // step 6 was 13.0 dB
        val s = Settings.fromJson(v1)
        assertEquals(-21, s.int(Catalog.KEY_MASTER_VOLUME))
        assertEquals(-12, s.int(Catalog.KEY_MASTER_LIMIT))
        assertEquals(130, s.int("bass.gain"))
    }

    @Test fun masterSlidersMapHalfDbToTheEngineGain() {
        assertEquals(100, Catalog.linearX100(0))
        assertEquals(50, Catalog.linearX100(-12))
        assertEquals(10, Catalog.linearX100(-40))
        assertEquals(200, Catalog.linearX100(12))

        assertEquals(0, Catalog.halfDb(100))
        assertEquals(-12, Catalog.halfDb(50))
        assertEquals(-21, Catalog.halfDb(30))
        assertEquals(-40, Catalog.halfDb(10))
    }

    @Test fun v3RoundTripKeepsTheMasterGain() {
        val s = Settings.DEFAULT.with(Catalog.KEY_MASTER_VOLUME, -13).with(Catalog.KEY_MASTER_LIMIT, -6)
        val back = Settings.fromJson(JSONObject(s.toJson().toString()))
        assertEquals(s, back)
        assertEquals(-13, back.int(Catalog.KEY_MASTER_VOLUME))
        assertEquals(-6, back.int(Catalog.KEY_MASTER_LIMIT))
    }

    @Test fun diffListsOnlyChangedKeys() {
        val a = Settings.DEFAULT
        val b = a.with("reverb.wet", 40).with(Catalog.KEY_MASTER, true)
        assertEquals(setOf("reverb.wet", Catalog.KEY_MASTER), b.diff(a))
        assertTrue(a.diff(a).isEmpty())
    }

    @Test fun rejectsWrongTypes() {
        runCatching { Settings.DEFAULT.with("bass.gain", "loud") }.onSuccess { fail("accepted a string for an int") }
    }

    @Test fun eqBandParsingIsTolerant() {
        assertArrayEquals(floatArrayOf(4f, 4f, 0.5f, 0f, 0f, 0f, 0f, 1.5f, -4f, -4.5f),
            Applier.parseBands("4.0;4.0;0.5;0.0;0.0;0.0;0.0;1.5;-4.0;-4.5;"), 0f)
        assertFalse(Applier.parseBands("garbage").any { it != 0f })
        assertEquals(12f, Applier.parseBands("99")[0])
    }
}
