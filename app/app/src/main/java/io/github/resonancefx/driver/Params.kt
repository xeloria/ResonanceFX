// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import java.util.UUID

/**
 * Wire protocol of the ResonanceFX engine: one AudioEffect with the UUID below, driven through
 * integer parameter IDs. The IDs and the UUID are kept stable so existing presets, .vdc headset
 * profiles and impulse responses keep working.
 */
object Params {
    val TYPE_NULL: UUID = UUID.fromString("ec7178ec-e5e1-4432-a3f4-4657e6795210")
    val UUID_ENGINE: UUID = UUID.fromString("4daa8eaa-0842-4a8d-82e9-6eee5827fbe4")

    // Getters
    const val GET_ENABLED = 1
    const val GET_CONFIGURE = 2
    const val GET_STREAMING = 3
    const val GET_SAMPLING_RATE = 4
    const val GET_CONVOLUTION_KERNEL_ID = 5
    const val GET_DRIVER_VERSION_CODE = 6
    const val GET_DRIVER_VERSION_NAME = 7
    const val GET_DISABLE_REASON = 8
    const val GET_DISABLE_REASON_MESSAGE = 9
    const val GET_CONFIG = 10
    const val GET_ARCHITECTURE = 11
    const val GET_CHANNEL_COUNT = 12 // ResonanceFX engine only
    const val GET_FEATURES = 13 // ResonanceFX engine only: bitmask of FEATURE_*
    const val GET_ABX_DELTA = 14 // listening test: loudness of B minus A, dB x100, or ABX_NOT_READY
    const val ABX_NOT_READY = Int.MIN_VALUE

    const val FEATURE_CHANNEL_COUNT = 0x1
    const val FEATURE_PARAMETRIC_EQ = 0x2
    const val FEATURE_LOUDNESS = 0x4
    const val FEATURE_USER_EQ = 0x8
    const val FEATURE_GRAPHIC_EQ = 0x10
    const val FEATURE_MULTIBAND = 0x20
    const val FEATURE_DYNAMIC_EQ = 0x40
    const val FEATURE_DE_ESSER = 0x80
    const val FEATURE_VOCAL = 0x100
    const val FEATURE_SPATIAL = 0x200
    const val FEATURE_TRUE_STEREO_KERNEL = 0x400
    const val FEATURE_SPEAKER_PROFILES = 0x800
    const val FEATURE_SPEAKER_PROTECTION = 0x1000
    const val FEATURE_CROSSTALK = 0x2000
    const val FEATURE_LEVEL_OFFSET = 0x4000
    const val FEATURE_ABX = 0x8000
    const val FEATURE_MEASURED_PROTECTION = 0x10000

    // Setters outside the effect range
    /** 1 = timestamp every processed buffer so GET_STREAMING works. */
    const val SET_UPDATE_STATUS = 36866
    /** Resets every module's DSP state (sent after a full apply, as the original app does). */
    const val SET_RESET_STATUS = 36867

    // Convolver
    const val CONVOLUTION_ENABLE = 65538
    const val CONVOLUTION_PREPARE_BUFFER = 65540
    const val CONVOLUTION_SET_BUFFER = 65541
    const val CONVOLUTION_COMMIT_BUFFER = 65542
    const val CONVOLUTION_CROSS_CHANNEL = 65543

    // Headphone surround (VHE)
    const val HEADPHONE_SURROUND_ENABLE = 65544
    const val HEADPHONE_SURROUND_STRENGTH = 65545

    // DDC
    const val DDC_ENABLE = 65546
    const val DDC_COEFFICIENTS = 65547

    // Spectrum extension
    const val SPECTRUM_EXTENSION_ENABLE = 65548
    const val SPECTRUM_EXTENSION_BARK = 65549
    const val SPECTRUM_EXTENSION_BARK_RECONSTRUCT = 65550

    // FIR equalizer
    const val FIR_EQUALIZER_ENABLE = 65551
    const val FIR_EQUALIZER_BAND_LEVEL = 65552

    // Field surround
    const val FIELD_SURROUND_ENABLE = 65553
    const val FIELD_SURROUND_MID_IMAGE = 65555
    const val FIELD_SURROUND_DEPTH = 65556

    // Differential surround
    const val DIFFERENTIAL_SURROUND_ENABLE = 65557
    const val DIFFERENTIAL_SURROUND_DELAY = 65558

    // Reverberation
    const val REVERBERATION_ENABLE = 65559
    const val REVERBERATION_ROOM_SIZE = 65560
    const val REVERBERATION_ROOM_WIDTH = 65561
    const val REVERBERATION_ROOM_DAMPENING = 65562
    const val REVERBERATION_ROOM_WET_SIGNAL = 65563
    const val REVERBERATION_ROOM_DRY_SIGNAL = 65564

    // Playback gain (AGC)
    const val AGC_ENABLE = 65565
    const val AGC_RATIO = 65566
    const val AGC_VOLUME = 65567
    const val AGC_MAX_SCALER = 65568

    // Dynamic system
    const val DYNAMIC_SYSTEM_ENABLE = 65569
    const val DYNAMIC_SYSTEM_X_COEFFICIENTS = 65570
    const val DYNAMIC_SYSTEM_Y_COEFFICIENTS = 65571
    const val DYNAMIC_SYSTEM_SIDE_GAIN = 65572
    const val DYNAMIC_SYSTEM_STRENGTH = 65573

    // Bass enhancer
    const val BASS_ENABLE = 65574
    const val BASS_MODE = 65575
    const val BASS_FREQUENCY = 65576
    const val BASS_GAIN = 65577

    // Clarity enhancer
    const val CLARITY_ENABLE = 65578
    const val CLARITY_MODE = 65579
    const val CLARITY_GAIN = 65580

    // Cure (auditory protection)
    const val CURE_ENABLE = 65581
    const val CURE_STRENGTH = 65582

    const val TUBE_SIMULATOR_ENABLE = 65583

    const val ANALOGX_ENABLE = 65584
    const val ANALOGX_MODE = 65585

    // Master gate
    const val GATE_OUTPUT_VOLUME = 65586
    const val GATE_CHANNEL_PAN = 65587
    const val GATE_LIMIT = 65588

    const val SPEAKER_OPTIMIZATION = 65603

    // FET compressor
    const val FET_ENABLE = 65610
    const val FET_THRESHOLD = 65611
    const val FET_RATIO = 65612
    const val FET_KNEE = 65613
    const val FET_AUTO_KNEE = 65614
    const val FET_GAIN = 65615
    const val FET_AUTO_GAIN = 65616
    const val FET_ATTACK = 65617
    const val FET_AUTO_ATTACK = 65618
    const val FET_RELEASE = 65619
    const val FET_AUTO_RELEASE = 65620
    const val FET_KNEE_MULTI = 65621
    const val FET_MAX_ATTACK = 65622
    const val FET_MAX_RELEASE = 65623
    const val FET_CREST = 65624
    const val FET_ADAPT = 65625
    const val FET_NO_CLIP = 65626

    // Parametric EQ (ResonanceFX engine only), used for AutoEq profiles.
    const val PARAMETRIC_EQ_ENABLE = 65700
    const val PARAMETRIC_EQ_FILTERS = 65701
    const val PARAMETRIC_EQ_MAX_FILTERS = 32

    // Engine 2.0 modules. Scalars are int32; "x100" values are fixed-point.
    const val LOUDNESS_ENABLE = 65710
    const val LOUDNESS_REFERENCE = 65711   // phon
    const val LOUDNESS_STRENGTH = 65712    // %
    const val LOUDNESS_VOLUME = 65713      // dBFS x100, runtime
    const val LOUDNESS_MODE = 65714        // 0 follow volume, 1 manual
    const val LOUDNESS_MANUAL = 65715      // phon
    const val LOUDNESS_CALIBRATION = 65716 // phon at full volume

    const val USER_EQ_ENABLE = 65730
    const val USER_EQ_FILTERS = 65731      // same payload as PARAMETRIC_EQ_FILTERS

    const val GRAPHIC_EQ_ENABLE = 65735
    const val GRAPHIC_EQ_LAYOUT = 65736    // 0 = 10, 1 = 15, 2 = 31 bands
    const val GRAPHIC_EQ_GAINS = 65737     // [n][gains dB]

    const val MULTIBAND_ENABLE = 65740
    const val MULTIBAND_SETTINGS = 65741   // [33][bands, knee, mix, autoMakeup, xo[4], thr[5], ratio[5], att[5], rel[5], gain[5]]

    const val DYNAMIC_EQ_ENABLE = 65745
    const val DYNAMIC_EQ_BANDS = 65746     // [n][n x (enabled, type, Hz, Q, thr, range, ratio, att, rel, mode)]

    const val DE_ESSER_ENABLE = 65750
    const val DE_ESSER_FREQ = 65751
    const val DE_ESSER_THRESHOLD = 65752   // dB x100
    const val DE_ESSER_AMOUNT = 65753      // dB x100
    const val DE_ESSER_MODE = 65754

    const val VOCAL_ENABLE = 65755
    const val VOCAL_CENTRE = 65756         // dB x100
    const val VOCAL_AMBIENCE = 65757       // dB x100
    const val VOCAL_LOW = 65758
    const val VOCAL_HIGH = 65759
    const val VOCAL_FOCUS = 65760          // %

    const val SPATIAL_ENABLE = 65765
    const val SPATIAL_ANGLE = 65766        // degrees
    const val SPATIAL_ROOM = 65767         // %
    const val SPATIAL_YAW = 65768          // degrees x100, runtime
    const val SPATIAL_TRACKING = 65769

    // Speaker correction, enabled by SPEAKER_OPTIMIZATION.
    const val SPEAKER_PROFILE = 65771      // 0 phone .. 6 TV, 7 measured
    const val SPEAKER_STRENGTH = 65772     // %
    const val SPEAKER_CORRECTION = 65773   // same payload as USER_EQ_FILTERS

    const val SPEAKER_PROTECT_ENABLE = 65775
    const val SPEAKER_PROTECT_RESONANCE = 65776 // Hz
    const val SPEAKER_PROTECT_LIMIT = 65777     // dB x100
    const val SPEAKER_PROTECT_CEILINGS = 65778  // [5][dBFS peak per band], measured mode
    const val SPEAKER_PROTECT_MODE = 65779      // 0 excursion model, 1 measured ceilings

    const val CROSSTALK_ENABLE = 65780
    const val CROSSTALK_AMOUNT = 65781     // %
    const val CROSSTALK_SPACING = 65782    // cm
    const val CROSSTALK_DISTANCE = 65783   // cm
    const val CROSSTALK_ORIENTATION = 65784 // % of the width applied, runtime (device orientation)

    const val AGC_MAX_CUT = 65785          // dB x100; 0 = only lift quiet programme
    const val INPUT_LEVEL_OFFSET = 65786   // dB x100, runtime: volume Android applied before us

    // Blind listening test: MODE 1 runs it; EXCLUDE is the enable id of the effect under test
    // (0 = all effects); SELECT 0 plays A (reference), 1 plays B (the chain as set).
    const val ABX_MODE = 65790
    const val ABX_EXCLUDE = 65791
    const val ABX_SELECT = 65792

    // Payload sizes the engine accepts for array parameters.
    const val DDC_PAYLOAD_BYTES = 1024
    const val KERNEL_CHUNK_PAYLOAD_BYTES = 8192
    const val KERNEL_CHUNK_DATA_BYTES = 8184
}
