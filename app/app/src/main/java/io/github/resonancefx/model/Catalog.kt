// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.model

import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Codec
import io.github.resonancefx.driver.EngineEffect
import io.github.resonancefx.driver.Params

/** A user-facing control and how its value reaches the engine. */
sealed interface Control {
    val key: String
    val label: String
}

class Toggle(
    override val key: String,
    override val label: String,
    val default: Boolean,
    val apply: (EngineEffect, Boolean) -> Unit,
) : Control

class Slider(
    override val key: String,
    override val label: String,
    val range: IntRange,
    val default: Int,
    val format: (Int) -> String,
    val apply: (EngineEffect, Int) -> Unit,
) : Control

class Choice(
    override val key: String,
    override val label: String,
    val options: List<String>,
    val default: Int,
    val apply: (EngineEffect, Int) -> Unit,
) : Control

/** 10-band FIR equalizer; value is "g0;g1;...;g9" in dB. */
class Equalizer(override val key: String, override val label: String) : Control

/** A file the user imports: DDC headset profile (.vdc) or convolution impulse response (.wav). */
class FileControl(override val key: String, override val label: String, val kind: Kind) : Control {
    enum class Kind(val directory: String, val extensions: List<String>) {
        DDC("ddc", listOf("vdc")),
        KERNEL("kernel", listOf("wav", "irs")),
    }
}

/** A headphone from the bundled AutoEq database; value is the profile id ("source/name"). */
class AutoEqControl(override val key: String, override val label: String) : Control

/** Graphic EQ whose band count comes from [layoutKey] (0 = 10, 1 = 15, 2 = 31); "g0;g1;..." dB. */
class GraphicEqControl(override val key: String, override val label: String, val layoutKey: String) : Control

/** User parametric EQ bands: "type,Hz,dB,Q;..." with type 0 peak, 1 low shelf, 2 high shelf,
 *  3 low-pass, 4 high-pass, 5 band-pass, 6 notch. */
class BandsControl(override val key: String, override val label: String) : Control

class Section(
    val id: String,
    val title: String,
    val summary: String,
    val enableKey: String?,
    val enableParam: Int?,
    val controls: List<Control>,
    val advanced: Boolean = false,
    /** Engine feature bit this section needs (older engines hide it); 0 = always available. */
    val feature: Int = 0,
    /**
     * Sections whose engine parameter packs several controls into one payload: called once
     * after any of the section's controls change (their own apply lambdas do nothing).
     */
    val compose: ((EngineEffect, Settings) -> Unit)? = null,
)

object Catalog {
    const val KEY_MASTER = "master.enable"
    const val KEY_AUTOEQ_ENABLE = "autoeq.enable"
    const val KEY_AUTOEQ_PROFILE = "autoeq.profile"
    const val KEY_AUTOEQ_STRENGTH = "autoeq.strength"

    val sections: List<Section> = listOf(
        Section(
            "master", "Master", "Output gain, balance and the final limiter.",
            enableKey = null, enableParam = null,
            controls = listOf(
                Slider("master.volume", "Output gain", 0..21, 11, { "${Tables.OUTPUT_VOLUME_DB[it]} dB" }) { fx, v ->
                    fx.set(Params.GATE_OUTPUT_VOLUME, Tables.OUTPUT_VOLUME_VALUES[v])
                },
                Slider("master.pan", "Balance", 0..100, 50, { panLabel(it) }) { fx, v ->
                    fx.set(Params.GATE_CHANNEL_PAN, v * 2 - 100)
                },
                Slider("master.limit", "Limiter threshold", 0..5, 5, { "${Tables.OUTPUT_DB[it]} dB" }) { fx, v ->
                    fx.set(Params.GATE_LIMIT, Tables.OUTPUT_DB_VALUES[v])
                },
            ),
        ),
        Section(
            "autoeq", "AutoEQ",
            "Measured correction for thousands of headphones — sound as their makers intended.",
            KEY_AUTOEQ_ENABLE, Params.PARAMETRIC_EQ_ENABLE,
            listOf(
                AutoEqControl(KEY_AUTOEQ_PROFILE, "Headphone"),
                // Applied together with the profile by the Applier (scales every filter gain).
                Slider(KEY_AUTOEQ_STRENGTH, "Strength", 0..100, 100, { "$it%" }) { _, _ -> },
            ),
        ),
        Section(
            "loud", "Loudness compensation",
            "Keeps bass and treble full at low volume, following ISO 226 equal-loudness contours.",
            "loud.enable", Params.LOUDNESS_ENABLE,
            listOf(
                Slider("loud.strength", "Strength", 0..100, 70, { "$it%" }) { fx, v -> fx.set(Params.LOUDNESS_STRENGTH, v) },
                Choice("loud.mode", "Listening level", listOf("Follow system volume", "Fixed"), 0) { fx, v ->
                    fx.set(Params.LOUDNESS_MODE, v)
                },
                Slider("loud.manual", "Fixed level", 30..90, 60, { "$it phon" }) { fx, v -> fx.set(Params.LOUDNESS_MANUAL, v) },
                Slider("loud.reference", "Reference level", 60..90, 80, { "$it phon" }) { fx, v ->
                    fx.set(Params.LOUDNESS_REFERENCE, v)
                },
                Slider("loud.calibration", "Level at full volume", 80..110, 100, { "$it phon" }) { fx, v ->
                    fx.set(Params.LOUDNESS_CALIBRATION, v)
                },
            ),
            feature = Params.FEATURE_LOUDNESS,
        ),
        Section(
            "peq", "Parametric EQ", "Your own filters: peaks, shelves, passes and notches, up to 32 bands.",
            "peq.enable", Params.USER_EQ_ENABLE,
            listOf(
                BandsControl("peq.bands", "Bands"),
                Slider("peq.preamp", "Preamp", -24..6, 0, { "$it dB" }) { _, _ -> },
            ),
            advanced = true,
            feature = Params.FEATURE_USER_EQ,
            compose = { fx, s ->
                val bands = Composite.parseBands(s.string("peq.bands"))
                fx.set(Params.USER_EQ_FILTERS, Codec.parametricEq(s.int("peq.preamp").toFloat(),
                    bands.map { floatArrayOf(it.type.toFloat(), it.frequency, it.gainDb, it.q) }))
            },
        ),
        Section(
            "geq", "Fine graphic EQ", "15 or 31 bands, each slider hit within a fraction of a dB.",
            "geq.enable", Params.GRAPHIC_EQ_ENABLE,
            listOf(
                Choice("geq.layout", "Bands", listOf("15 bands (2/3 octave)", "31 bands (1/3 octave)"), 1) { _, _ -> },
                GraphicEqControl("geq.gains", "Gains", "geq.layout"),
            ),
            advanced = true,
            feature = Params.FEATURE_GRAPHIC_EQ,
            compose = { fx, s ->
                val layout = s.int("geq.layout") + 1  // engine layouts: 1 = 15, 2 = 31 bands
                val gains = Composite.parseGains(s.string("geq.gains"), Composite.graphicEqBands(layout).size)
                fx.set(Params.GRAPHIC_EQ_LAYOUT, layout)
                fx.set(Params.GRAPHIC_EQ_GAINS, Codec.floatArray(gains.size, gains))
            },
        ),
        Section(
            "agc", "Playback gain control", "Levels quiet and loud tracks automatically.",
            "agc.enable", Params.AGC_ENABLE,
            listOf(
                Choice("agc.ratio", "Strength", listOf("Gentle", "Medium", "Strong"), 0) { fx, v ->
                    fx.set(Params.AGC_RATIO, Tables.AGC_RATIO_VALUES[v])
                },
                Slider("agc.volume", "Target level", 0..5, 0, { "${Tables.OUTPUT_DB[it]} dB" }) { fx, v ->
                    fx.set(Params.AGC_VOLUME, Tables.OUTPUT_DB_VALUES[v])
                },
                Slider("agc.max", "Maximum gain", 0..10, 3, { "${Tables.MULTI_FACTOR_VALUES[it] / 100}x" }) { fx, v ->
                    fx.set(Params.AGC_MAX_SCALER, Tables.MULTI_FACTOR_VALUES[v])
                },
            ),
        ),
        Section(
            "fet", "FET compressor", "Studio-style compressor with automatic attack, release and knee.",
            "fet.enable", Params.FET_ENABLE,
            listOf(
                Slider("fet.threshold", "Threshold", 0..100, 100, ::percent) { fx, v -> fx.set(Params.FET_THRESHOLD, v) },
                Slider("fet.ratio", "Ratio", 0..100, 100, ::percent) { fx, v -> fx.set(Params.FET_RATIO, v) },
                Toggle("fet.autoKnee", "Automatic knee", true) { fx, v -> fx.set(Params.FET_AUTO_KNEE, v) },
                Slider("fet.knee", "Knee width", 0..100, 0, ::percent) { fx, v -> fx.set(Params.FET_KNEE, v) },
                Slider("fet.kneeMulti", "Knee multiplier", 0..100, 0, ::percent) { fx, v -> fx.set(Params.FET_KNEE_MULTI, v) },
                Toggle("fet.autoGain", "Automatic makeup gain", true) { fx, v -> fx.set(Params.FET_AUTO_GAIN, v) },
                Slider("fet.gain", "Makeup gain", 0..100, 0, ::percent) { fx, v -> fx.set(Params.FET_GAIN, v) },
                Toggle("fet.autoAttack", "Automatic attack", true) { fx, v -> fx.set(Params.FET_AUTO_ATTACK, v) },
                Slider("fet.attack", "Attack", 1..200, 20, { "$it ms" }) { fx, v -> fx.set(Params.FET_ATTACK, v) },
                Slider("fet.maxAttack", "Maximum attack", 1..200, 80, { "$it ms" }) { fx, v -> fx.set(Params.FET_MAX_ATTACK, v) },
                Toggle("fet.autoRelease", "Automatic release", false) { fx, v -> fx.set(Params.FET_AUTO_RELEASE, v) },
                Slider("fet.release", "Release", 1..200, 50, { "$it ms" }) { fx, v -> fx.set(Params.FET_RELEASE, v) },
                Slider("fet.maxRelease", "Maximum release", 1..200, 100, { "$it ms" }) { fx, v -> fx.set(Params.FET_MAX_RELEASE, v) },
                Slider("fet.crest", "Crest", 0..200, 100, ::percent) { fx, v -> fx.set(Params.FET_CREST, v) },
                Slider("fet.adapt", "Adaptation", 0..100, 50, ::percent) { fx, v -> fx.set(Params.FET_ADAPT, v) },
                Toggle("fet.noClip", "Prevent clipping", true) { fx, v -> fx.set(Params.FET_NO_CLIP, v) },
            ),
            advanced = true,
        ),
        Section(
            "ddc", "Headphone correction", "Frequency-response correction from a .vdc headset profile.",
            "ddc.enable", Params.DDC_ENABLE,
            listOf(FileControl("ddc.file", "Headset profile", FileControl.Kind.DDC)),
        ),
        Section(
            "vse", "Spectrum extension", "Restores high frequencies lost to lossy compression.",
            "vse.enable", Params.SPECTRUM_EXTENSION_ENABLE,
            listOf(
                Slider("vse.strength", "Strength", 0..100, 10, ::percent) { fx, v ->
                    fx.set(Params.SPECTRUM_EXTENSION_BARK, 7600)
                    fx.set(Params.SPECTRUM_EXTENSION_BARK_RECONSTRUCT, (v * 5.6).toInt())
                },
            ),
        ),
        Section(
            "eq", "Equalizer", "10-band equalizer with ${Tables.EQ_PRESETS.size} presets.",
            "eq.enable", Params.FIR_EQUALIZER_ENABLE,
            listOf(Equalizer("eq.bands", "Bands")),
        ),
        Section(
            "conv", "Convolver", "Applies an impulse response: rooms, speakers, headphone targets.",
            "conv.enable", Params.CONVOLUTION_ENABLE,
            listOf(
                FileControl("conv.file", "Impulse response", FileControl.Kind.KERNEL),
                Slider("conv.cross", "Cross-channel mix", 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.CONVOLUTION_CROSS_CHANNEL, v)
                },
            ),
        ),
        Section(
            "mbc", "Multiband compressor",
            "Compresses lows, mids and highs separately: denser, more even sound without pumping.",
            "mbc.enable", Params.MULTIBAND_ENABLE,
            listOf(
                Choice("mbc.bands", "Bands", listOf("3 bands", "4 bands", "5 bands"), 0) { _, _ -> },
                Slider("mbc.threshold", "Threshold", -40..0, -18, { "$it dB" }) { _, _ -> },
                Slider("mbc.ratio", "Ratio", 10..80, 25, { "%.1f:1".format(it / 10f) }) { _, _ -> },
                Slider("mbc.speed", "Speed", 0..100, 50, { "$it%" }) { _, _ -> },
                Slider("mbc.mix", "Mix", 0..100, 100, { "$it%" }) { _, _ -> },
                Toggle("mbc.makeup", "Automatic makeup gain", true) { _, _ -> },
            ),
            advanced = true,
            feature = Params.FEATURE_MULTIBAND,
            compose = { fx, s -> fx.set(Params.MULTIBAND_SETTINGS, Composite.multiband(s)) },
        ),
        Section(
            "deq", "Dynamic EQ", "EQ that only acts when needed: tames boom and harshness, lifts detail at low levels.",
            "deq.enable", Params.DYNAMIC_EQ_ENABLE,
            listOf(
                Slider("deq.boom", "Tame boomy bass", 0..12, 4, { if (it == 0) "Off" else "-$it dB" }) { _, _ -> },
                Slider("deq.harsh", "Tame harshness", 0..12, 4, { if (it == 0) "Off" else "-$it dB" }) { _, _ -> },
                Slider("deq.quietBass", "Lift bass in quiet passages", 0..12, 0, { if (it == 0) "Off" else "+$it dB" }) { _, _ -> },
                Slider("deq.quietAir", "Lift air in quiet passages", 0..12, 0, { if (it == 0) "Off" else "+$it dB" }) { _, _ -> },
            ),
            advanced = true,
            feature = Params.FEATURE_DYNAMIC_EQ,
            compose = { fx, s -> fx.set(Params.DYNAMIC_EQ_BANDS, Composite.dynamicEq(s)) },
        ),
        Section(
            "deess", "De-esser", "Softens sharp \"s\" and \"t\" sounds.",
            "deess.enable", Params.DE_ESSER_ENABLE,
            listOf(
                Slider("deess.freq", "Frequency", 3000..12000, 6500, { "%.1f kHz".format(it / 1000f) }) { fx, v ->
                    fx.set(Params.DE_ESSER_FREQ, v)
                },
                Slider("deess.threshold", "Threshold", -50..0, -28, { "$it dB" }) { fx, v -> fx.set(Params.DE_ESSER_THRESHOLD, v * 100) },
                Slider("deess.amount", "Maximum reduction", 0..20, 8, { "$it dB" }) { fx, v -> fx.set(Params.DE_ESSER_AMOUNT, v * 100) },
                Choice("deess.mode", "Mode", listOf("Split band", "Wide band"), 0) { fx, v -> fx.set(Params.DE_ESSER_MODE, v) },
            ),
            feature = Params.FEATURE_DE_ESSER,
        ),
        Section(
            "vocal", "Vocals & ambience",
            "Separates the centre (voices) from the surroundings: dialogue boost or karaoke. Adds ~21 ms latency.",
            "vocal.enable", Params.VOCAL_ENABLE,
            listOf(
                Slider("vocal.centre", "Vocals", -40..12, 4, { if (it <= -40) "Removed" else "%+d dB".format(it) }) { fx, v ->
                    fx.set(Params.VOCAL_CENTRE, v * 100)
                },
                Slider("vocal.ambience", "Ambience", -40..12, 0, { if (it <= -40) "Removed" else "%+d dB".format(it) }) { fx, v ->
                    fx.set(Params.VOCAL_AMBIENCE, v * 100)
                },
                Slider("vocal.focus", "Focus", 0..100, 50, { "$it%" }) { fx, v ->
                    fx.set(Params.VOCAL_FOCUS, v)
                    fx.set(Params.VOCAL_LOW, 120)
                    fx.set(Params.VOCAL_HIGH, 10000)
                },
            ),
            feature = Params.FEATURE_VOCAL,
        ),
        Section(
            "spatial", "Spatial audio",
            "Virtual speakers in a room for headphones, measured on a real head (MIT KEMAR), with head tracking.",
            "spatial.enable", Params.SPATIAL_ENABLE,
            listOf(
                Slider("spatial.angle", "Speaker angle", 10..90, 30, { "±$it°" }) { fx, v -> fx.set(Params.SPATIAL_ANGLE, v) },
                Slider("spatial.room", "Room", 0..100, 30, { "$it%" }) { fx, v -> fx.set(Params.SPATIAL_ROOM, v) },
                Toggle("spatial.tracking", "Head tracking (supported headphones)", false) { fx, v ->
                    fx.set(Params.SPATIAL_TRACKING, v)
                },
            ),
            feature = Params.FEATURE_SPATIAL,
        ),
        Section(
            "field", "Field surround", "Widens the stereo image.",
            "field.enable", Params.FIELD_SURROUND_ENABLE,
            listOf(
                Slider("field.depth", "Surround strength", 0..8, 0, { "${it + 1}" }) { fx, v ->
                    fx.set(Params.FIELD_SURROUND_DEPTH, v * 75 + 200)
                },
                Slider("field.mid", "Mid image", 0..10, 5, { "${it * 10}%" }) { fx, v ->
                    fx.set(Params.FIELD_SURROUND_MID_IMAGE, v * 10 + 100)
                },
            ),
        ),
        Section(
            "diff", "Differential surround", "Haas-effect spaciousness.",
            "diff.enable", Params.DIFFERENTIAL_SURROUND_ENABLE,
            listOf(
                Slider("diff.delay", "Delay", 0..19, 4, { "${it + 1} ms" }) { fx, v ->
                    fx.set(Params.DIFFERENTIAL_SURROUND_DELAY, (v + 1) * 100)
                },
            ),
        ),
        Section(
            "vhe", "Headphone surround+", "Virtual speakers for headphones.",
            "vhe.enable", Params.HEADPHONE_SURROUND_ENABLE,
            listOf(
                Slider("vhe.strength", "Quality", 0..4, 0, { "${it + 1}" }) { fx, v ->
                    fx.set(Params.HEADPHONE_SURROUND_STRENGTH, v)
                },
            ),
        ),
        Section(
            "reverb", "Reverberation", "Room ambience.",
            "reverb.enable", Params.REVERBERATION_ENABLE,
            listOf(
                Slider("reverb.size", "Room size", 0..10, 0, { "${Tables.REVERB_ROOM_SIZE[it]} m²" }) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_SIZE, v * 10)
                },
                Slider("reverb.width", "Sound field", 0..10, 0, { "${Tables.REVERB_ROOM_WIDTH[it]} m" }) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_WIDTH, v * 10)
                },
                Slider("reverb.damp", "Damping", 0..100, 0, ::percent) { fx, v -> fx.set(Params.REVERBERATION_ROOM_DAMPENING, v) },
                Slider("reverb.wet", "Wet signal", 0..100, 0, ::percent) { fx, v -> fx.set(Params.REVERBERATION_ROOM_WET_SIGNAL, v) },
                Slider("reverb.dry", "Dry signal", 0..100, 50, ::percent) { fx, v -> fx.set(Params.REVERBERATION_ROOM_DRY_SIGNAL, v) },
            ),
        ),
        Section(
            "dyn", "Dynamic system", "Dynamic bass tuned to your headphone type.",
            "dyn.enable", Params.DYNAMIC_SYSTEM_ENABLE,
            listOf(
                Choice("dyn.device", "Device type", Tables.DYNAMIC_SYSTEM_DEVICES.map { it.first }, 4) { fx, v ->
                    val c = Tables.DYNAMIC_SYSTEM_DEVICES[v].second
                    fx.set(Params.DYNAMIC_SYSTEM_X_COEFFICIENTS, c[0], c[1])
                    fx.set(Params.DYNAMIC_SYSTEM_Y_COEFFICIENTS, c[2], c[3])
                    fx.set(Params.DYNAMIC_SYSTEM_SIDE_GAIN, c[4], c[5])
                },
                Slider("dyn.strength", "Bass strength", 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.DYNAMIC_SYSTEM_STRENGTH, v * 20 + 100)
                },
            ),
        ),
        Section(
            "tube", "Tube simulator", "6N1J vacuum-tube warmth.",
            "tube.enable", Params.TUBE_SIMULATOR_ENABLE, emptyList(),
        ),
        Section(
            "bass", "Bass enhancer", "Deep, controlled low end.",
            "bass.enable", Params.BASS_ENABLE,
            listOf(
                Choice("bass.mode", "Mode", listOf("Natural", "Pure bass+", "Subwoofer"), 0) { fx, v -> fx.set(Params.BASS_MODE, v) },
                Slider("bass.freq", "Frequency", 0..135, 55, { "${it + 15} Hz" }) { fx, v -> fx.set(Params.BASS_FREQUENCY, v + 15) },
                Slider("bass.gain", "Gain", 0..11, 0, { "${Tables.BASS_BOOST_DB[it]} dB" }) { fx, v -> fx.set(Params.BASS_GAIN, v * 50 + 50) },
            ),
        ),
        Section(
            "clarity", "Clarity enhancer", "Treble detail and vocal presence.",
            "clarity.enable", Params.CLARITY_ENABLE,
            listOf(
                Choice("clarity.mode", "Mode", listOf("Natural", "OZone+", "XHiFi"), 0) { fx, v -> fx.set(Params.CLARITY_MODE, v) },
                Slider("clarity.gain", "Gain", 0..9, 1, { "${Tables.CLARITY_BOOST_DB[it]} dB" }) { fx, v -> fx.set(Params.CLARITY_GAIN, v * 50) },
            ),
        ),
        Section(
            "cure", "Auditory protection", "Crossfeed that reduces listening fatigue on headphones.",
            "cure.enable", Params.CURE_ENABLE,
            listOf(
                Choice("cure.strength", "Binaural level", listOf("Mild", "Medium", "Strong"), 0) { fx, v -> fx.set(Params.CURE_STRENGTH, v) },
            ),
        ),
        Section(
            "analogx", "AnalogX", "Analog-console harmonic coloration.",
            "analogx.enable", Params.ANALOGX_ENABLE,
            listOf(
                Choice("analogx.mode", "Level", listOf("Light", "Medium", "Heavy"), 0) { fx, v -> fx.set(Params.ANALOGX_MODE, v) },
            ),
        ),
        Section(
            "speaker", "Speaker optimization", "Compensates for small phone speakers.",
            "speaker.enable", Params.SPEAKER_OPTIMIZATION, emptyList(),
        ),
    )

    /** Every key with its default value; a profile only stores what differs. */
    val defaults: Map<String, Any> by lazy {
        buildMap {
            put(KEY_MASTER, false)
            sections.forEach { s ->
                s.enableKey?.let { put(it, false) }
                s.controls.forEach { c ->
                    when (c) {
                        is Toggle -> put(c.key, c.default)
                        is Slider -> put(c.key, c.default)
                        is Choice -> put(c.key, c.default)
                        is Equalizer -> put(c.key, Tables.EQ_PRESETS[0].second)
                        is FileControl -> put(c.key, "")
                        is AutoEqControl -> put(c.key, "")
                        is GraphicEqControl -> put(c.key, "")
                        is BandsControl -> put(c.key, "")
                    }
                }
            }
        }
    }

    val controlsByKey: Map<String, Pair<Section, Control>> by lazy {
        sections.flatMap { s -> s.controls.map { it.key to (s to it) } }.toMap()
    }

    /** Composite sections by any of their control keys. */
    val composites: Map<String, Section> by lazy {
        sections.filter { it.compose != null }.flatMap { s -> s.controls.map { it.key to s } }.toMap()
    }

    val sectionsByEnableKey: Map<String, Section> by lazy {
        sections.filter { it.enableKey != null }.associateBy { it.enableKey!! }
    }

    private fun percent(v: Int) = "$v%"

    private fun panLabel(v: Int) = when {
        v == 50 -> "Center"
        v < 50 -> "L ${(50 - v) * 2}%"
        else -> "R ${(v - 50) * 2}%"
    }
}
