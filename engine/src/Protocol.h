// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

// ResonanceFX parameter protocol.
//
// One AudioEffect instance, driven through EFFECT_CMD_SET_PARAM / GET_PARAM with a 32-bit
// parameter id. Scalar values are int32; arrays travel in fixed-size payloads (see below).
// The ids, the effect UUID and the meaning of each value are what the app and saved presets
// depend on, so they never change; new features only add ids.
//
// Value conventions for the original ids follow what the app presents to the user (e.g.
// output gain 100 = 0 dB, i.e. linear gain x100). Ids >= 65700 are ResonanceFX extensions;
// unless stated otherwise their scalars are fixed-point x100.

#include <cstdint>

namespace rfx::proto {

// ---- Effect identity -------------------------------------------------------------------------
// {4daa8eaa-0842-4a8d-82e9-6eee5827fbe4}, type EFFECT_TYPE_NULL.
inline constexpr uint32_t kUuidTimeLow = 0x4daa8eaa;
inline constexpr uint16_t kUuidTimeMid = 0x0842;
inline constexpr uint16_t kUuidTimeHi = 0x4a8d;
inline constexpr uint16_t kUuidClockSeq = 0x82e9;
inline constexpr uint8_t kUuidNode[6] = {0x6e, 0xee, 0x58, 0x27, 0xfb, 0xe4};

// ---- Getters -----------------------------------------------------------------------------------
enum Get : int32_t {
    kGetEnabled = 1,
    kGetConfigured = 2,
    kGetStreaming = 3,          // processed audio within the last 5 s
    kGetSampleRate = 4,
    kGetKernelId = 5,           // id given with the last committed convolution kernel
    kGetVersionCode = 6,
    kGetVersionName = 7,
    kGetDisableReason = 8,
    kGetDisableMessage = 9,
    kGetConfig = 10,            // 40-byte input/output buffer_config summary
    kGetArchitecture = 11,
    kGetChannelCount = 12,
    kGetFeatures = 13,          // bitmask of kFeature*
    kGetAbxDelta = 14,          // listening test: loudness of B minus A, dB x100 (kAbxNotReady until measured)
};

inline constexpr int32_t kAbxNotReady = INT32_MIN;

enum Feature : uint32_t {
    kFeatureChannelCount = 1u << 0,
    kFeatureParametricEq = 1u << 1,
    kFeatureLoudness = 1u << 2,
    kFeatureUserEq = 1u << 3,
    kFeatureGraphicEq = 1u << 4,
    kFeatureMultiband = 1u << 5,
    kFeatureDynamicEq = 1u << 6,
    kFeatureDeEsser = 1u << 7,
    kFeatureVocal = 1u << 8,
    kFeatureSpatial = 1u << 9,
    kFeatureTrueStereoKernel = 1u << 10,  // 4-path convolution kernels
    kFeatureSpeakerProfiles = 1u << 11,   // speaker correction profiles + measured correction
    kFeatureSpeakerProtection = 1u << 12,
    kFeatureCrosstalk = 1u << 13,
    kFeatureLevelOffset = 1u << 14,  // kInputLevelOffset and kAgcMaxCut
    kFeatureAbx = 1u << 15,          // blind listening test (kAbx*)
    kFeatureMeasuredProtection = 1u << 16,  // kSpeakerProtectCeilings / kSpeakerProtectMode
};

// ---- Housekeeping -----------------------------------------------------------------------------
enum Control : int32_t {
    kSetUpdateStatus = 0x9002,  // accepted for compatibility; streaming status is always tracked
    kSetResetStatus = 0x9003,   // clears every module's signal state (adaptive gains are kept)
};

// ---- Effects -----------------------------------------------------------------------------------
enum Param : int32_t {
    // Convolver. PREPARE (bufferId, paths 1|2|4, clear) / SET_BUFFER 8192-byte payload
    // [bufferId][floatCount][floats] / COMMIT (bufferId, crc32 of all floats, kernelId).
    kConvEnable = 65538,
    kConvPrepare = 65540,
    kConvSetBuffer = 65541,
    kConvCommit = 65542,
    kConvCrossChannel = 65543,  // 0..100 %

    kHeadphoneSurroundEnable = 65544,
    kHeadphoneSurroundLevel = 65545,  // 0..4

    // Headphone correction from .vdc profiles. 1024-byte payload [n][n coefs @44.1k][n @48k],
    // n = 5 per biquad: b0 b1 b2 a1 a2 with y = b0x + b1x1 + b2x2 + a1y1 + a2y2.
    kDdcEnable = 65546,
    kDdcCoefficients = 65547,

    kSpectrumExtEnable = 65548,
    kSpectrumExtReference = 65549,  // Hz above which content is regenerated
    kSpectrumExtAmount = 65550,     // 0..560 (strength x5.6)

    kEqEnable = 65551,
    kEqBand = 65552,  // (band 0..9, gain in 0.01 dB)

    kFieldSurroundEnable = 65553,
    kFieldSurroundMidImage = 65555,  // 100..200 -> mid level 0..100 %
    kFieldSurroundDepth = 65556,     // 200..800 -> strength 1..9

    kDiffSurroundEnable = 65557,
    kDiffSurroundDelay = 65558,  // 100..2000 -> 1..20 ms

    kReverbEnable = 65559,
    kReverbRoomSize = 65560,  // 0..100
    kReverbWidth = 65561,     // 0..100
    kReverbDamping = 65562,   // 0..100
    kReverbWet = 65563,       // 0..100
    kReverbDry = 65564,       // 0..100 (50 = unity)

    kAgcEnable = 65565,
    kAgcRatio = 65566,      // 50 | 100 | 300: gentle | medium | strong
    kAgcVolume = 65567,     // 30..100: target level, linear x100 relative to the reference
    kAgcMaxScaler = 65568,  // 100..3000: maximum gain, linear x100

    kDynamicSystemEnable = 65569,
    kDynamicSystemX = 65570,     // (low-mid corner Hz, presence corner Hz)
    kDynamicSystemY = 65571,     // (bass Q x100, bass corner Hz)
    kDynamicSystemSide = 65572,  // (low-mid trim %, presence trim %)
    kDynamicSystemStrength = 65573,  // 100..2100

    kBassEnable = 65574,
    kBassMode = 65575,  // 0 natural, 1 psychoacoustic, 2 subwoofer
    kBassFreq = 65576,  // Hz
    kBassGain = 65577,  // 50..600 (50 per step of the app's boost table)

    kClarityEnable = 65578,
    kClarityMode = 65579,  // 0 natural, 1 exciter, 2 hi-fi extension
    kClarityGain = 65580,  // 0..450 (50 per step of the app's boost table)

    kCrossfeedEnable = 65581,
    kCrossfeedLevel = 65582,  // 0..2: BS2B default, Chu Moy, Jan Meier

    kTubeEnable = 65583,

    kAnalogEnable = 65584,
    kAnalogLevel = 65585,  // 0..2

    kOutputVolume = 65586,  // linear gain x100 (1..200)
    kChannelPan = 65587,    // -100..100
    kLimiterCeiling = 65588,  // linear x100 (30..100)

    kSpeakerOptimization = 65603,  // enables the speaker correction (kSpeakerProfile)

    kCompEnable = 65610,
    kCompThreshold = 65611,   // 0..100 -> -60..0 dB
    kCompRatio = 65612,       // 0..100 -> 1:1..20:1
    kCompKnee = 65613,        // 0..100 -> 0..24 dB
    kCompAutoKnee = 65614,
    kCompGain = 65615,        // 0..100 -> 0..24 dB makeup
    kCompAutoGain = 65616,
    kCompAttack = 65617,      // ms
    kCompAutoAttack = 65618,
    kCompRelease = 65619,     // ms
    kCompAutoRelease = 65620,
    kCompKneeMulti = 65621,   // 0..100: extra knee width with auto knee
    kCompMaxAttack = 65622,   // ms
    kCompMaxRelease = 65623,  // ms
    kCompCrest = 65624,       // 0..200 %: auto-timing sensitivity to crest factor
    kCompAdapt = 65625,       // 0..100: auto-timing adaptation speed
    kCompNoClip = 65626,

    // ---- ResonanceFX extensions --------------------------------------------------------------
    // AutoEQ parametric EQ. 1024-byte payload [n][preamp dB][n x (type, Hz, dB, Q)] floats.
    // type: 0 peak, 1 low shelf, 2 high shelf (and the other FilterType values, see below).
    kAutoEqEnable = 65700,
    kAutoEqFilters = 65701,

    // Loudness compensation (ISO 226:2023 equal-loudness contours).
    kLoudnessEnable = 65710,
    kLoudnessReference = 65711,  // reference listening level, phon
    kLoudnessStrength = 65712,   // 0..100 %
    kLoudnessVolume = 65713,     // current system volume, dBFS x100 (runtime)
    kLoudnessMode = 65714,       // 0 follow volume, 1 manual
    kLoudnessManual = 65715,     // manual listening level, phon
    kLoudnessCalibration = 65716,  // phon at full volume

    // User parametric EQ: same payload as kAutoEqFilters; type is any FilterType value
    // (0 peak, 1 low shelf, 2 high shelf, 3 low-pass, 4 high-pass, 5 band-pass, 6 notch,
    // 7 all-pass, 8/9 first-order shelves, 10/11 first-order passes, 12 tilt).
    kUserEqEnable = 65730,
    kUserEqFilters = 65731,

    // Graphic EQ: layout 0 = 10, 1 = 15, 2 = 31 bands; GAINS is a 256/1024-byte payload
    // [n][n gains dB] floats.
    kGraphicEqEnable = 65735,
    kGraphicEqLayout = 65736,
    kGraphicEqGains = 65737,

    // Multiband compressor. SETTINGS is a 1024-byte payload [count][floats]:
    // bands, knee dB, mix 0..1, autoMakeup 0|1, crossovers[4] Hz, threshold[5] dB, ratio[5],
    // attack[5] ms, release[5] ms, gain[5] dB.
    kMultibandEnable = 65740,
    kMultibandSettings = 65741,

    // Dynamic EQ. BANDS is a 1024-byte payload [n][n x (enabled, type, Hz, Q, threshold dB,
    // range dB, ratio, attack ms, release ms, mode)] floats, n <= 8.
    kDynamicEqEnable = 65745,
    kDynamicEqBands = 65746,

    kDeEsserEnable = 65750,
    kDeEsserFreq = 65751,       // Hz
    kDeEsserThreshold = 65752,  // dB x100
    kDeEsserAmount = 65753,     // dB x100
    kDeEsserMode = 65754,       // 0 split band, 1 wide band

    kVocalEnable = 65755,
    kVocalCentre = 65756,    // dB x100
    kVocalAmbience = 65757,  // dB x100
    kVocalLow = 65758,       // Hz
    kVocalHigh = 65759,      // Hz
    kVocalFocus = 65760,     // 0..100 %

    kSpatialEnable = 65765,
    kSpatialAngle = 65766,  // speaker angle, degrees
    kSpatialRoom = 65767,   // 0..100 %
    kSpatialYaw = 65768,    // head yaw, degrees x100 (runtime, from head tracking)
    kSpatialTracking = 65769,

    // Speaker correction (enabled by kSpeakerOptimization). Profile 0 phone, 1 tablet, 2 laptop,
    // 3 small Bluetooth speaker, 4 large Bluetooth / home speaker, 5 car, 6 TV, 7 measured.
    // CORRECTION has the kUserEqFilters payload: the bands fitted to the user's measurement.
    kSpeakerProfile = 65771,
    kSpeakerStrength = 65772,  // 0..100 %
    kSpeakerCorrection = 65773,

    // Speaker protection: excursion limiter.
    kSpeakerProtectEnable = 65775,
    kSpeakerProtectResonance = 65776,  // driver resonance, Hz
    kSpeakerProtectLimit = 65777,      // excursion ceiling, dB x100 (-3000..0)
    // Measured mode: 1024-byte payload [5][5 ceilings, dBFS peak] for the bands below 125 Hz,
    // 125-250, 250-500, 500-1000 and above 1 kHz (>= 0: no limit), from the user's distortion
    // measurement; MODE 1 uses them, 0 the excursion model.
    kSpeakerProtectCeilings = 65778,
    kSpeakerProtectMode = 65779,

    // Crosstalk cancellation for closely spaced speakers.
    kCrosstalkEnable = 65780,
    kCrosstalkAmount = 65781,    // 0..100 %
    kCrosstalkSpacing = 65782,   // speaker spacing, cm
    kCrosstalkDistance = 65783,  // listening distance, cm
    kCrosstalkOrientation = 65784,  // 0..100 % of the width applied (runtime: follows how the device is held)

    // Playback gain control: how far loud programme may be turned down, dB x100 (0 = only lift
    // quiet programme).
    kAgcMaxCut = 65785,
    // Gain the system applied before the engine, dB x100 (<= 0; runtime). On outputs where
    // Android scales the mix by the volume before effects, the app sends that volume so the
    // dynamics modules judge the programme's own level.
    kInputLevelOffset = 65786,

    // Blind listening test. While MODE is 1 a second engine renders the reference (A): every
    // effect off (EXCLUDE 0), or only the effect whose enable id EXCLUDE names. SELECT 0 plays A,
    // 1 plays B (the chain as set); both are time-aligned and loudness-matched.
    kAbxMode = 65790,
    kAbxExclude = 65791,
    kAbxSelect = 65792,
};

}  // namespace rfx::proto
