// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.model

import android.content.Context
import androidx.annotation.StringRes
import io.github.resonancefx.R
import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Codec
import io.github.resonancefx.driver.EngineEffect
import io.github.resonancefx.driver.Params
import java.util.Locale

/**
 * Every user-facing string is a resource id, resolved when it is drawn, so the catalogue itself
 * never holds one language. Value formats take the drawing context.
 */

/** A user-facing control and how its value reaches the engine. */
sealed interface Control {
    val key: String

    @get:StringRes
    val label: Int
}

class Toggle(
    override val key: String,
    override val label: Int,
    val default: Boolean,
    val apply: (EngineEffect, Boolean) -> Unit,
) : Control

typealias ValueFormat = (Context, Int) -> String

class Slider(
    override val key: String,
    override val label: Int,
    val range: IntRange,
    val default: Int,
    val format: ValueFormat,
    val apply: (EngineEffect, Int) -> Unit,
) : Control

class Choice(
    override val key: String,
    override val label: Int,
    /** One string resource per option; the index is what gets persisted, so never reorder. */
    val options: List<Int>,
    val default: Int,
    val apply: (EngineEffect, Int) -> Unit,
) : Control

/** 10-band FIR equalizer; value is "g0;g1;...;g9" in dB. */
class Equalizer(override val key: String, override val label: Int) : Control

/** A file the user imports: DDC headset profile (.vdc) or convolution impulse response (.wav). */
class FileControl(override val key: String, override val label: Int, val kind: Kind) : Control {
    enum class Kind(val directory: String, val extensions: List<String>) {
        DDC("ddc", listOf("vdc")),
        KERNEL("kernel", listOf("wav", "irs")),
    }
}

/** A headphone from the bundled AutoEq database; value is the profile id ("source/name"). */
class AutoEqControl(override val key: String, override val label: Int) : Control

/** Graphic EQ whose band count comes from [layoutKey] (0 = 10, 1 = 15, 2 = 31); "g0;g1;..." dB. */
class GraphicEqControl(override val key: String, override val label: Int, val layoutKey: String) : Control

/** User parametric EQ bands: "type,Hz,dB,Q;..." with type 0 peak, 1 low shelf, 2 high shelf,
 *  3 low-pass, 4 high-pass, 5 band-pass, 6 notch. */
class BandsControl(override val key: String, override val label: Int) : Control

/**
 * The user's own speaker measurement and the correction fitted to it (see [Composite.Measurement]);
 * [matchKey] holds the profile of the speaker it is matched to ("" = the natural target).
 */
class MeasureControl(override val key: String, override val label: Int, val matchKey: String) : Control

class Section(
    val id: String,
    @StringRes val title: Int,
    @StringRes val summary: Int,
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
    /** Settings of other sections that [compose] also reads (it re-runs when they change). */
    val dependsOn: Set<String> = emptySet(),
)

object Catalog {
    const val KEY_MASTER = "master.enable"
    const val KEY_AUTOEQ_ENABLE = "autoeq.enable"
    const val KEY_AUTOEQ_PROFILE = "autoeq.profile"
    const val KEY_AUTOEQ_STRENGTH = "autoeq.strength"
    const val KEY_SPEAKER_ENABLE = "speaker.enable"
    const val KEY_SPEAKER_PROFILE = "speaker.profile"
    const val KEY_SPEAKER_MEASURED = "speaker.measured"
    const val KEY_SPEAKER_MATCH = "speaker.match"
    const val KEY_XTC_SIDEWAYS = "xtc.sideways"
    const val KEY_PROTECT_SIZE = "protect.size"

    /** Engine speaker profiles, in protocol order; the last one plays the user's measurement. */
    val SPEAKER_PROFILES = listOf(
        R.string.speaker_phone, R.string.speaker_tablet, R.string.speaker_laptop,
        R.string.speaker_small_bluetooth, R.string.speaker_large_bluetooth, R.string.speaker_car,
        R.string.speaker_tv, R.string.speaker_measured,
    )
    val PROFILE_MEASURED = SPEAKER_PROFILES.lastIndex

    /** Speaker classes for protection, with the driver resonance each one implies (Hz). */
    val SPEAKER_SIZES = listOf(
        R.string.speaker_phone to 700, R.string.speaker_tablet_laptop to 400,
        R.string.speaker_small_bluetooth to 150, R.string.speaker_large_bluetooth to 70,
        R.string.speaker_car_hifi to 40,
    )

    val sections: List<Section> = listOf(
        Section(
            "master", R.string.section_master_title, R.string.section_master_summary,
            enableKey = null, enableParam = null,
            controls = listOf(
                Slider("master.volume", R.string.label_output_gain, 0..21, 11, { c, v ->
                    c.getString(R.string.value_db, Tables.OUTPUT_VOLUME_DB[v])
                }) { fx, v ->
                    fx.set(Params.GATE_OUTPUT_VOLUME, Tables.OUTPUT_VOLUME_VALUES[v])
                },
                Slider("master.pan", R.string.label_balance, 0..100, 50, ::panLabel) { fx, v ->
                    fx.set(Params.GATE_CHANNEL_PAN, v * 2 - 100)
                },
                Slider("master.limit", R.string.label_limiter_threshold, 0..5, 5, { c, v ->
                    c.getString(R.string.value_db, Tables.OUTPUT_DB[v])
                }) { fx, v ->
                    fx.set(Params.GATE_LIMIT, Tables.OUTPUT_DB_VALUES[v])
                },
            ),
        ),
        Section(
            "autoeq", R.string.section_autoeq_title, R.string.section_autoeq_summary,
            KEY_AUTOEQ_ENABLE, Params.PARAMETRIC_EQ_ENABLE,
            listOf(
                AutoEqControl(KEY_AUTOEQ_PROFILE, R.string.label_headphone),
                // Applied together with the profile by the Applier (scales every filter gain).
                Slider(KEY_AUTOEQ_STRENGTH, R.string.label_strength, 0..100, 100, ::percent) { _, _ -> },
            ),
        ),
        Section(
            "loud", R.string.section_loud_title, R.string.section_loud_summary,
            "loud.enable", Params.LOUDNESS_ENABLE,
            listOf(
                Slider("loud.strength", R.string.label_strength, 0..100, 70, ::percent) { fx, v ->
                    fx.set(Params.LOUDNESS_STRENGTH, v)
                },
                Choice("loud.mode", R.string.label_listening_level, listOf(
                    R.string.option_follow_system_volume, R.string.option_fixed,
                ), 0) { fx, v ->
                    fx.set(Params.LOUDNESS_MODE, v)
                },
                Slider("loud.manual", R.string.label_fixed_level, 30..90, 60, ::phon) { fx, v ->
                    fx.set(Params.LOUDNESS_MANUAL, v)
                },
                Slider("loud.reference", R.string.label_reference_level, 60..90, 80, ::phon) { fx, v ->
                    fx.set(Params.LOUDNESS_REFERENCE, v)
                },
                Slider("loud.calibration", R.string.label_level_full_volume, 80..110, 100, ::phon) { fx, v ->
                    fx.set(Params.LOUDNESS_CALIBRATION, v)
                },
            ),
            feature = Params.FEATURE_LOUDNESS,
        ),
        Section(
            "peq", R.string.section_peq_title, R.string.section_peq_summary,
            "peq.enable", Params.USER_EQ_ENABLE,
            listOf(
                BandsControl("peq.bands", R.string.label_bands),
                Slider("peq.preamp", R.string.label_preamp, -24..6, 0, ::db) { _, _ -> },
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
            "geq", R.string.section_geq_title, R.string.section_geq_summary,
            "geq.enable", Params.GRAPHIC_EQ_ENABLE,
            listOf(
                Choice("geq.layout", R.string.label_bands, listOf(
                    R.string.option_15_bands, R.string.option_31_bands,
                ), 1) { _, _ -> },
                GraphicEqControl("geq.gains", R.string.label_gains, "geq.layout"),
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
            "agc", R.string.section_agc_title, R.string.section_agc_summary,
            "agc.enable", Params.AGC_ENABLE,
            listOf(
                Choice("agc.ratio", R.string.label_speed, listOf(
                    R.string.option_gentle, R.string.option_medium, R.string.option_fast,
                ), 0) { fx, v ->
                    fx.set(Params.AGC_RATIO, Tables.AGC_RATIO_VALUES[v])
                },
                Slider("agc.volume", R.string.label_target_loudness, 0..5, 5, { c, v ->
                    c.getString(R.string.value_lufs, oneDecimal(-14f + Tables.OUTPUT_DB[v].toFloat()))
                }) { fx, v ->
                    fx.set(Params.AGC_VOLUME, Tables.OUTPUT_DB_VALUES[v])
                },
                Slider("agc.cut", R.string.label_turn_down, 0..12, 0, { c, v ->
                    if (v == 0) c.getString(R.string.value_never) else c.getString(R.string.value_up_to_db, v)
                }) { fx, v ->
                    fx.set(Params.AGC_MAX_CUT, v * 100)
                },
                Slider("agc.max", R.string.label_max_gain, 0..10, 3, { c, v ->
                    c.getString(R.string.value_x, (Tables.MULTI_FACTOR_VALUES[v] / 100).toString())
                }) { fx, v ->
                    fx.set(Params.AGC_MAX_SCALER, Tables.MULTI_FACTOR_VALUES[v])
                },
            ),
        ),
        Section(
            "fet", R.string.section_fet_title, R.string.section_fet_summary,
            "fet.enable", Params.FET_ENABLE,
            listOf(
                Slider("fet.threshold", R.string.label_threshold, 0..100, 100, ::percent) { fx, v ->
                    fx.set(Params.FET_THRESHOLD, v)
                },
                Slider("fet.ratio", R.string.label_ratio, 0..100, 100, ::percent) { fx, v ->
                    fx.set(Params.FET_RATIO, v)
                },
                Toggle("fet.autoKnee", R.string.label_auto_knee, true) { fx, v -> fx.set(Params.FET_AUTO_KNEE, v) },
                Slider("fet.knee", R.string.label_knee_width, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.FET_KNEE, v)
                },
                Slider("fet.kneeMulti", R.string.label_knee_multiplier, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.FET_KNEE_MULTI, v)
                },
                Toggle("fet.autoGain", R.string.label_auto_makeup, true) { fx, v -> fx.set(Params.FET_AUTO_GAIN, v) },
                Slider("fet.gain", R.string.label_makeup_gain, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.FET_GAIN, v)
                },
                Toggle("fet.autoAttack", R.string.label_auto_attack, true) { fx, v -> fx.set(Params.FET_AUTO_ATTACK, v) },
                Slider("fet.attack", R.string.label_attack, 1..200, 20, ::millis) { fx, v -> fx.set(Params.FET_ATTACK, v) },
                Slider("fet.maxAttack", R.string.label_max_attack, 1..200, 80, ::millis) { fx, v ->
                    fx.set(Params.FET_MAX_ATTACK, v)
                },
                Toggle("fet.autoRelease", R.string.label_auto_release, false) { fx, v -> fx.set(Params.FET_AUTO_RELEASE, v) },
                Slider("fet.release", R.string.label_release, 1..200, 50, ::millis) { fx, v -> fx.set(Params.FET_RELEASE, v) },
                Slider("fet.maxRelease", R.string.label_max_release, 1..200, 100, ::millis) { fx, v ->
                    fx.set(Params.FET_MAX_RELEASE, v)
                },
                Slider("fet.crest", R.string.label_crest, 0..200, 100, ::percent) { fx, v -> fx.set(Params.FET_CREST, v) },
                Slider("fet.adapt", R.string.label_adaptation, 0..100, 50, ::percent) { fx, v -> fx.set(Params.FET_ADAPT, v) },
                Toggle("fet.noClip", R.string.label_prevent_clipping, true) { fx, v -> fx.set(Params.FET_NO_CLIP, v) },
            ),
            advanced = true,
        ),
        Section(
            "ddc", R.string.section_ddc_title, R.string.section_ddc_summary,
            "ddc.enable", Params.DDC_ENABLE,
            listOf(FileControl("ddc.file", R.string.label_headset_profile, FileControl.Kind.DDC)),
        ),
        Section(
            "vse", R.string.section_vse_title, R.string.section_vse_summary,
            "vse.enable", Params.SPECTRUM_EXTENSION_ENABLE,
            listOf(
                Slider("vse.strength", R.string.label_strength, 0..100, 10, ::percent) { fx, v ->
                    fx.set(Params.SPECTRUM_EXTENSION_BARK, 7600)
                    fx.set(Params.SPECTRUM_EXTENSION_BARK_RECONSTRUCT, (v * 5.6).toInt())
                },
            ),
        ),
        Section(
            "eq", R.string.section_eq_title, R.string.section_eq_summary,
            "eq.enable", Params.FIR_EQUALIZER_ENABLE,
            listOf(Equalizer("eq.bands", R.string.label_bands)),
        ),
        Section(
            "conv", R.string.section_conv_title, R.string.section_conv_summary,
            "conv.enable", Params.CONVOLUTION_ENABLE,
            listOf(
                FileControl("conv.file", R.string.label_impulse_response, FileControl.Kind.KERNEL),
                Slider("conv.cross", R.string.label_cross_channel_mix, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.CONVOLUTION_CROSS_CHANNEL, v)
                },
            ),
        ),
        Section(
            "mbc", R.string.section_mbc_title, R.string.section_mbc_summary,
            "mbc.enable", Params.MULTIBAND_ENABLE,
            listOf(
                Choice("mbc.bands", R.string.label_bands, listOf(
                    R.string.option_3_bands, R.string.option_4_bands, R.string.option_5_bands,
                ), 0) { _, _ -> },
                Slider("mbc.threshold", R.string.label_threshold, -40..0, -18, ::db) { _, _ -> },
                Slider("mbc.ratio", R.string.label_ratio, 10..80, 25, { c, v ->
                    c.getString(R.string.value_ratio, oneDecimal(v / 10f))
                }) { _, _ -> },
                Slider("mbc.speed", R.string.label_speed, 0..100, 50, ::percent) { _, _ -> },
                Slider("mbc.mix", R.string.label_mix, 0..100, 100, ::percent) { _, _ -> },
                Toggle("mbc.makeup", R.string.label_auto_makeup, true) { _, _ -> },
            ),
            advanced = true,
            feature = Params.FEATURE_MULTIBAND,
            compose = { fx, s -> fx.set(Params.MULTIBAND_SETTINGS, Composite.multiband(s)) },
        ),
        Section(
            "deq", R.string.section_deq_title, R.string.section_deq_summary,
            "deq.enable", Params.DYNAMIC_EQ_ENABLE,
            listOf(
                Slider("deq.boom", R.string.label_tame_boomy_bass, 0..12, 4, ::offOrMinus) { _, _ -> },
                Slider("deq.harsh", R.string.label_tame_harshness, 0..12, 4, ::offOrMinus) { _, _ -> },
                Slider("deq.quietBass", R.string.label_lift_bass_quiet, 0..12, 0, ::offOrPlus) { _, _ -> },
                Slider("deq.quietAir", R.string.label_lift_air_quiet, 0..12, 0, ::offOrPlus) { _, _ -> },
            ),
            advanced = true,
            feature = Params.FEATURE_DYNAMIC_EQ,
            compose = { fx, s -> fx.set(Params.DYNAMIC_EQ_BANDS, Composite.dynamicEq(s)) },
        ),
        Section(
            "deess", R.string.section_deess_title, R.string.section_deess_summary,
            "deess.enable", Params.DE_ESSER_ENABLE,
            listOf(
                Slider("deess.freq", R.string.label_frequency, 3000..12000, 6500, { c, v ->
                    c.getString(R.string.value_khz, oneDecimal(v / 1000f))
                }) { fx, v ->
                    fx.set(Params.DE_ESSER_FREQ, v)
                },
                Slider("deess.threshold", R.string.label_threshold, -50..0, -28, ::db) { fx, v ->
                    fx.set(Params.DE_ESSER_THRESHOLD, v * 100)
                },
                Slider("deess.amount", R.string.label_max_reduction, 0..20, 8, ::db) { fx, v ->
                    fx.set(Params.DE_ESSER_AMOUNT, v * 100)
                },
                Choice("deess.mode", R.string.label_mode, listOf(
                    R.string.option_split_band, R.string.option_wide_band,
                ), 0) { fx, v -> fx.set(Params.DE_ESSER_MODE, v) },
            ),
            feature = Params.FEATURE_DE_ESSER,
        ),
        Section(
            "vocal", R.string.section_vocal_title, R.string.section_vocal_summary,
            "vocal.enable", Params.VOCAL_ENABLE,
            listOf(
                Slider("vocal.centre", R.string.label_vocals, -40..12, 4, ::removedOrDb) { fx, v ->
                    fx.set(Params.VOCAL_CENTRE, v * 100)
                },
                Slider("vocal.ambience", R.string.label_ambience, -40..12, 0, ::removedOrDb) { fx, v ->
                    fx.set(Params.VOCAL_AMBIENCE, v * 100)
                },
                Slider("vocal.focus", R.string.label_focus, 0..100, 50, ::percent) { fx, v ->
                    fx.set(Params.VOCAL_FOCUS, v)
                    fx.set(Params.VOCAL_LOW, 120)
                    fx.set(Params.VOCAL_HIGH, 10000)
                },
            ),
            feature = Params.FEATURE_VOCAL,
        ),
        Section(
            "spatial", R.string.section_spatial_title, R.string.section_spatial_summary,
            "spatial.enable", Params.SPATIAL_ENABLE,
            listOf(
                Slider("spatial.angle", R.string.label_speaker_angle, 10..90, 30, { c, v ->
                    c.getString(R.string.value_degrees, v)
                }) { fx, v -> fx.set(Params.SPATIAL_ANGLE, v) },
                Slider("spatial.room", R.string.label_room, 0..100, 30, ::percent) { fx, v ->
                    fx.set(Params.SPATIAL_ROOM, v)
                },
                Toggle("spatial.tracking", R.string.label_head_tracking, false) { fx, v ->
                    fx.set(Params.SPATIAL_TRACKING, v)
                },
            ),
            feature = Params.FEATURE_SPATIAL,
        ),
        Section(
            "field", R.string.section_field_title, R.string.section_field_summary,
            "field.enable", Params.FIELD_SURROUND_ENABLE,
            listOf(
                Slider("field.depth", R.string.label_surround_strength, 0..8, 0, { _, v ->
                    "${v + 1}"
                }) { fx, v ->
                    fx.set(Params.FIELD_SURROUND_DEPTH, v * 75 + 200)
                },
                Slider("field.mid", R.string.label_mid_image, 0..10, 5, { c, v ->
                    c.getString(R.string.value_percent, v * 10)
                }) { fx, v ->
                    fx.set(Params.FIELD_SURROUND_MID_IMAGE, v * 10 + 100)
                },
            ),
        ),
        Section(
            "diff", R.string.section_diff_title, R.string.section_diff_summary,
            "diff.enable", Params.DIFFERENTIAL_SURROUND_ENABLE,
            listOf(
                Slider("diff.delay", R.string.label_delay, 0..19, 4, { c, v ->
                    c.getString(R.string.value_ms, v + 1)
                }) { fx, v ->
                    fx.set(Params.DIFFERENTIAL_SURROUND_DELAY, (v + 1) * 100)
                },
                Slider("diff.width", R.string.label_width, 0..200, 100, { c, v ->
                    c.getString(R.string.value_percent, v)
                }) { fx, v ->
                    fx.set(Params.DIFFERENTIAL_SURROUND_WIDTH, v)
                },
            ),
        ),
        Section(
            "vhe", R.string.section_vhe_title, R.string.section_vhe_summary,
            "vhe.enable", Params.HEADPHONE_SURROUND_ENABLE,
            listOf(
                Slider("vhe.strength", R.string.label_quality, 0..4, 0, { _, v -> "${v + 1}" }) { fx, v ->
                    fx.set(Params.HEADPHONE_SURROUND_STRENGTH, v)
                },
            ),
        ),
        Section(
            "reverb", R.string.section_reverb_title, R.string.section_reverb_summary,
            "reverb.enable", Params.REVERBERATION_ENABLE,
            listOf(
                Slider("reverb.size", R.string.label_room_size, 0..10, 0, { c, v ->
                    c.getString(R.string.value_m2, Tables.REVERB_ROOM_SIZE[v])
                }) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_SIZE, v * 10)
                },
                Slider("reverb.width", R.string.label_sound_field, 0..10, 0, { c, v ->
                    c.getString(R.string.value_m, Tables.REVERB_ROOM_WIDTH[v])
                }) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_WIDTH, v * 10)
                },
                Slider("reverb.damp", R.string.label_damping, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_DAMPENING, v)
                },
                Slider("reverb.wet", R.string.label_wet, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_WET_SIGNAL, v)
                },
                Slider("reverb.dry", R.string.label_dry, 0..100, 50, ::percent) { fx, v ->
                    fx.set(Params.REVERBERATION_ROOM_DRY_SIGNAL, v)
                },
            ),
        ),
        Section(
            "dyn", R.string.section_dyn_title, R.string.section_dyn_summary,
            "dyn.enable", Params.DYNAMIC_SYSTEM_ENABLE,
            listOf(
                Choice("dyn.device", R.string.label_device_type, Tables.DYNAMIC_SYSTEM_DEVICES.map { it.first }, 4) { fx, v ->
                    val c = Tables.DYNAMIC_SYSTEM_DEVICES[v].second
                    fx.set(Params.DYNAMIC_SYSTEM_X_COEFFICIENTS, c[0], c[1])
                    fx.set(Params.DYNAMIC_SYSTEM_Y_COEFFICIENTS, c[2], c[3])
                    fx.set(Params.DYNAMIC_SYSTEM_SIDE_GAIN, c[4], c[5])
                },
                Slider("dyn.strength", R.string.label_bass_strength, 0..100, 0, ::percent) { fx, v ->
                    fx.set(Params.DYNAMIC_SYSTEM_STRENGTH, v * 20 + 100)
                },
            ),
        ),
        Section(
            "tube", R.string.section_tube_title, R.string.section_tube_summary,
            "tube.enable", Params.TUBE_SIMULATOR_ENABLE, emptyList(),
        ),
        Section(
            "bass", R.string.section_bass_title, R.string.section_bass_summary,
            "bass.enable", Params.BASS_ENABLE,
            listOf(
                Choice("bass.mode", R.string.label_mode, listOf(
                    R.string.option_natural, R.string.option_pure_bass, R.string.option_subwoofer,
                ), 0) { fx, v -> fx.set(Params.BASS_MODE, v) },
                Slider("bass.freq", R.string.label_frequency, 0..135, 55, { c, v ->
                    c.getString(R.string.value_hz, v + 15)
                }) { fx, v -> fx.set(Params.BASS_FREQUENCY, v + 15) },
                Slider("bass.gain", R.string.label_gain, 0..170, 35, { c, v ->
                    c.getString(R.string.value_db, oneDecimal(v / 10f))
                }) { fx, v -> fx.set(Params.BASS_GAIN, v) },
                Slider("bass.harmonics", R.string.label_harmonics, 0..50, 25, { _, v ->
                    String.format(Locale.ROOT, "%.2f", v / 100f)
                }) { fx, v -> fx.set(Params.BASS_HARMONICS, v) },
            ),
        ),
        Section(
            "clarity", R.string.section_clarity_title, R.string.section_clarity_summary,
            "clarity.enable", Params.CLARITY_ENABLE,
            listOf(
                Choice("clarity.mode", R.string.label_mode, listOf(
                    R.string.option_natural, R.string.option_ozone, R.string.option_xhifi,
                ), 0) { fx, v -> fx.set(Params.CLARITY_MODE, v) },
                Slider("clarity.gain", R.string.label_gain, 0..9, 1, { c, v ->
                    c.getString(R.string.value_db, Tables.CLARITY_BOOST_DB[v])
                }) { fx, v -> fx.set(Params.CLARITY_GAIN, v * 50) },
            ),
        ),
        Section(
            "cure", R.string.section_cure_title, R.string.section_cure_summary,
            "cure.enable", Params.CURE_ENABLE,
            listOf(
                Choice("cure.strength", R.string.label_binaural_level, listOf(
                    R.string.option_mild, R.string.option_medium, R.string.option_strong,
                ), 0) { fx, v -> fx.set(Params.CURE_STRENGTH, v) },
            ),
        ),
        Section(
            "analogx", R.string.section_analogx_title, R.string.section_analogx_summary,
            "analogx.enable", Params.ANALOGX_ENABLE,
            listOf(
                Choice("analogx.mode", R.string.label_level, listOf(
                    R.string.option_light, R.string.option_medium, R.string.option_heavy,
                ), 0) { fx, v -> fx.set(Params.ANALOGX_MODE, v) },
            ),
        ),
        Section(
            "speaker", R.string.section_speaker_title, R.string.section_speaker_summary,
            "speaker.enable", Params.SPEAKER_OPTIMIZATION,
            listOf(
                Choice(KEY_SPEAKER_PROFILE, R.string.label_speaker, SPEAKER_PROFILES, 0) { _, _ -> },
                Slider("speaker.strength", R.string.label_strength, 0..100, 100, ::percent) { _, _ -> },
                MeasureControl(KEY_SPEAKER_MEASURED, R.string.label_your_measurement, KEY_SPEAKER_MATCH),
            ),
            feature = Params.FEATURE_SPEAKER_PROFILES,
            compose = { fx, s ->
                fx.set(Params.SPEAKER_CORRECTION, Composite.speakerCorrection(s.string(KEY_SPEAKER_MEASURED)))
                fx.set(Params.SPEAKER_PROFILE, s.int(KEY_SPEAKER_PROFILE))
                fx.set(Params.SPEAKER_STRENGTH, s.int("speaker.strength"))
            },
        ),
        Section(
            "protect", R.string.section_protect_title, R.string.section_protect_summary,
            "protect.enable", Params.SPEAKER_PROTECT_ENABLE,
            listOf(
                Choice(KEY_PROTECT_SIZE, R.string.label_speaker,
                    SPEAKER_SIZES.map { it.first } + R.string.speaker_measured_distortion, 0) { _, _ -> },
                Slider("protect.limit", R.string.label_bass_ceiling, -24..-3, -12, ::db) { _, _ -> },
            ),
            feature = Params.FEATURE_SPEAKER_PROTECTION,
            dependsOn = setOf(KEY_SPEAKER_MEASURED),
            compose = { fx, s ->
                val size = s.int(KEY_PROTECT_SIZE)
                // The measured limits come from the distortion test in "Speaker correction".
                val ceilings = if (size == SPEAKER_SIZES.size) Composite.protectionCeilings(s.string(KEY_SPEAKER_MEASURED)) else null
                if (ceilings != null) {
                    fx.set(Params.SPEAKER_PROTECT_CEILINGS, ceilings)
                    fx.set(Params.SPEAKER_PROTECT_MODE, 1)
                } else {
                    fx.set(Params.SPEAKER_PROTECT_MODE, 0)
                    fx.set(Params.SPEAKER_PROTECT_RESONANCE, SPEAKER_SIZES[size.coerceAtMost(SPEAKER_SIZES.lastIndex)].second)
                    fx.set(Params.SPEAKER_PROTECT_LIMIT, s.int("protect.limit") * 100)
                }
            },
        ),
        Section(
            "xtc", R.string.section_xtc_title, R.string.section_xtc_summary,
            "xtc.enable", Params.CROSSTALK_ENABLE,
            listOf(
                Slider("xtc.amount", R.string.label_width, 0..100, 60, ::percent) { fx, v ->
                    fx.set(Params.CROSSTALK_AMOUNT, v)
                },
                Slider("xtc.spacing", R.string.label_spacing, 4..60, 15, { c, v ->
                    c.getString(R.string.value_cm, v)
                }) { fx, v -> fx.set(Params.CROSSTALK_SPACING, v) },
                Slider("xtc.distance", R.string.label_distance, 20..200, 40, { c, v ->
                    c.getString(R.string.value_cm, v)
                }) { fx, v ->
                    fx.set(Params.CROSSTALK_DISTANCE, v)
                },
                // Read by the service, which follows the device's orientation.
                Toggle(KEY_XTC_SIDEWAYS, R.string.label_sideways, true) { _, _ -> },
            ),
            feature = Params.FEATURE_CROSSTALK,
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
                        is MeasureControl -> {
                            put(c.key, "")
                            put(c.matchKey, "")
                        }
                    }
                }
            }
        }
    }

    val controlsByKey: Map<String, Pair<Section, Control>> by lazy {
        sections.flatMap { s -> s.controls.map { it.key to (s to it) } }.toMap()
    }

    /** Composite sections by any of their control keys (and the keys they depend on). */
    val composites: Map<String, List<Section>> by lazy {
        sections.filter { it.compose != null }
            .flatMap { s -> (s.controls.map { it.key } + s.dependsOn).map { it to s } }
            .groupBy({ it.first }, { it.second })
    }

    val sectionsByEnableKey: Map<String, Section> by lazy {
        sections.filter { it.enableKey != null }.associateBy { it.enableKey!! }
    }

    fun oneDecimal(v: Float) = String.format(Locale.ROOT, "%.1f", v)

    private fun db(c: Context, v: Int) = c.getString(R.string.value_db_int, v)
    private fun percent(c: Context, v: Int) = c.getString(R.string.value_percent, v)
    private fun phon(c: Context, v: Int) = c.getString(R.string.value_phon, v)
    private fun millis(c: Context, v: Int) = c.getString(R.string.value_ms, v)

    private fun offOrMinus(c: Context, v: Int) =
        if (v == 0) c.getString(R.string.value_off) else c.getString(R.string.value_negative_db, v)

    private fun offOrPlus(c: Context, v: Int) =
        if (v == 0) c.getString(R.string.value_off) else c.getString(R.string.value_positive_db, v)

    private fun removedOrDb(c: Context, v: Int) =
        if (v <= -40) c.getString(R.string.value_removed) else c.getString(R.string.value_db_signed, v)

    private fun panLabel(c: Context, v: Int) = when {
        v == 50 -> c.getString(R.string.value_center)
        v < 50 -> c.getString(R.string.value_pan_left, (50 - v) * 2)
        else -> c.getString(R.string.value_pan_right, (v - 50) * 2)
    }
}
