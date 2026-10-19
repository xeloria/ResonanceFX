// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#include "Engine.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>

#include "Protocol.h"
#include "rfx/dsp/Common.h"
#include "rfx/modules/Loudness.h"

namespace rfx {

using namespace proto;

namespace {

uint32_t crc32(const uint8_t* data, size_t length) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/// Interpolates `table` (dB per step) at a protocol value v = base + step * index.
float tableDb(const float* table, int size, int v, int base, int step) {
    const float x = clampv(static_cast<float>(v - base) / static_cast<float>(step), 0.0f, static_cast<float>(size - 1));
    const int i = std::min(static_cast<int>(x), size - 2);
    const float f = x - static_cast<float>(i);
    return table[i] + (table[i + 1] - table[i]) * f;
}

// The app's bass and clarity boost steps (what its sliders show).
constexpr float kBassBoostDb[] = {3.5f, 6.0f, 8.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 14.8f, 15.6f, 16.3f, 17.0f};
constexpr float kClarityBoostDb[] = {0.0f, 3.5f, 6.0f, 8.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 14.8f};

float linearX100ToDb(int v) { return static_cast<float>(gainToDb(std::max(v, 1) / 100.0)); }

uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

constexpr int kConvBlock = 512;
constexpr size_t kMaxKernelFloats = 4u * 48000u * 12u;  // 4 paths x 12 s at 48 kHz

}  // namespace

Engine::Engine() { prepare(48000.0); }

void Engine::prepare(double sampleRate) {
    fs_ = sampleRate > 0 ? sampleRate : 48000.0;
    stageStep_ = static_cast<float>(1.0 / (0.02 * fs_));
    dryL_.assign(kMaxBlock, 0.0f);
    dryR_.assign(kMaxBlock, 0.0f);

    ddc_.prepare(fs_);
    for (ParametricEq* eq : {&autoEq_, &userEq_, &legacyEq_, &geq_, &loud_, &clarityEq_, &vseEq_, &tubeEq_, &hrtfEq_}) {
        eq->prepare(fs_);
        eq->setMatched(true);
    }
    hrtfCompKey_ = -1.0;
    {
        // Tube warmth: the valve stage's gentle treble roll-off.
        EqBand b{true, FilterType::HighShelf, 8000.0f, -2.5f, 0.7071f, 0};
        tubeEq_.setBands(&b, 1);
    }
    deq_.prepare(fs_);
    deess_.prepare(fs_);
    vocal_.prepare(fs_);
    vse_.prepare(fs_);
    clarity_.prepare(fs_);
    bass_.prepare(fs_);
    sub_.prepare(fs_);
    dyn_.prepare(fs_);
    tube_.prepare(fs_);
    analog_.prepare(fs_);
    comp_.prepare(fs_);
    compClip_.prepare(fs_);
    mbc_.prepare(fs_);
    field_.prepare(fs_);
    haas_.prepare(fs_);
    cure_.prepare(fs_);
    hrtf_.prepare(fs_);
    reverb_.prepare(fs_);
    speaker_.prepare(fs_);
    agc_.prepare(fs_);
    limiter_.prepare(fs_);

    // Re-derive everything that depends on the rate.
    applyBass();
    applyClarity();
    applyComp();
    applyAgc();
    applyReverb();
    applyLoudness();
    applyLegacyEq();
    applyGraphicEq();
    applySpatial();
    applySpectrumExtension();
    field_.set((fieldDepth_ - 200) / 600.0f, (fieldMid_ - 100) / 100.0f);
    {
        Limiter::Settings ls;
        ls.ceilingDb = limiterCeilingDb_;
        limiter_.set(ls);
    }
    haas_.setDelayMs(haasMs_);
    analog_.setLevel(analogLevel_);
    dyn_.set(dynSettings_);
    deess_.set(deessSettings_);
    vocal_.set(vocalSettings_);
    cure_.set(cureSettings_);
    {
        // Valve character without audible distortion at normal levels.
        Tube::Settings ts;
        ts.drive = 0.12f;
        ts.even = 0.7f;
        ts.mix = 0.35f;
        tube_.set(ts);
    }
    outGain_.snap(targetGain_);
    // Convolution: rebuild the kernel for this rate's block layout.
    conv_.prepare(kConvBlock, 1);
    if (!kernelSamples_.empty() && kernelPaths_ > 0) {
        const uint32_t id = kernelId_, crc = kernelCrc_;
        pendingKernel_ = kernelSamples_;
        pendingPaths_ = kernelPaths_;
        pendingBufferId_ = 1;
        kernelCrc_ = 0;
        commitKernel(1, crc, id);
    }
    reset();
}

void Engine::reset() {
    for (Stage* st : {&stDdc_, &stAutoEq_, &stConv_, &stUserEq_, &stLegacyEq_, &stGeq_, &stLoud_, &stDeq_, &stDeess_,
                      &stVocal_, &stVse_, &stBass_, &stDyn_, &stClarity_, &stTube_, &stAnalog_, &stComp_, &stMbc_,
                      &stField_, &stHaas_, &stCure_, &stHrtf_, &stReverb_, &stSpeaker_, &stAgc_}) {
        st->mix = st->target ? 1.0f : 0.0f;
    }
    ddc_.reset();
    for (ParametricEq* eq : {&autoEq_, &userEq_, &legacyEq_, &geq_, &loud_, &clarityEq_, &vseEq_, &tubeEq_, &hrtfEq_}) eq->reset();
    conv_.reset();
    deq_.reset();
    deess_.reset();
    vocal_.reset();
    vse_.reset();
    clarity_.reset();
    bass_.reset();
    sub_.reset();
    dyn_.reset();
    tube_.reset();
    analog_.reset();
    comp_.reset();
    compClip_.reset();
    mbc_.reset();
    field_.reset();
    haas_.reset();
    cure_.reset();
    hrtf_.reset();
    reverb_.reset();
    speaker_.reset();
    agc_.reset();
    limiter_.reset();
    outGain_.snap(targetGain_);
}

// ---- Parameters ---------------------------------------------------------------------------------

int Engine::setParameter(int32_t id, const uint8_t* value, uint32_t size) {
    if (value == nullptr) return -EINVAL;
    // Array parameters arrive in fixed-size payloads.
    switch (id) {
        case kDdcCoefficients:
        case kConvSetBuffer:
        case kAutoEqFilters:
        case kUserEqFilters:
        case kGraphicEqGains:
        case kMultibandSettings:
        case kDynamicEqBands:
            return setPayload(id, value, size);
        default:
            break;
    }
    if (size < 4 || size % 4 != 0 || size > 16) return -EINVAL;
    int32_t v[4] = {};
    std::memcpy(v, value, size);
    return setInts(id, v, static_cast<int>(size / 4));
}

int Engine::setInts(int32_t id, const int32_t* v, int count) {
    const int a = v[0];
    const bool on = a != 0;
    switch (id) {
        case kSetUpdateStatus: return 0;
        case kSetResetStatus: reset(); return 0;

        case kConvEnable: stConv_.target = on; return 0;
        case kConvPrepare: {
            if (count < 3) return -EINVAL;
            pendingKernel_.clear();
            if (v[2] != 0) {  // clear the kernel
                pendingBufferId_ = 0;
                conv_.setKernel(nullptr);
                retiredKernel_ = std::move(kernel_);
                kernelSamples_.clear();
                kernelPaths_ = 0;
                kernelId_ = kernelCrc_ = 0;
                return 0;
            }
            const int paths = v[1];
            if (paths != 1 && paths != 2 && paths != 4) {
                pendingBufferId_ = 0;
                return -EINVAL;
            }
            pendingBufferId_ = static_cast<uint32_t>(v[0]);
            pendingPaths_ = paths;
            return 0;
        }
        case kConvCommit:
            if (count < 3) return -EINVAL;
            return commitKernel(static_cast<uint32_t>(v[0]), static_cast<uint32_t>(v[1]), static_cast<uint32_t>(v[2]))
                       ? 0
                       : -EINVAL;
        case kConvCrossChannel: crossChannel_ = clampv(a, 0, 100); return 0;

        case kHeadphoneSurroundEnable: spatial_p_.vhe = on; applySpatial(); return 0;
        case kHeadphoneSurroundLevel: spatial_p_.vheLevel = clampv(a, 0, 4); applySpatial(); return 0;

        case kDdcEnable: stDdc_.target = on; return 0;

        case kSpectrumExtEnable: stVse_.target = on; return 0;
        case kSpectrumExtReference:
        case kSpectrumExtAmount:
            if (id == kSpectrumExtReference) spectrumRef_ = clampv(a, 2000, 20000);
            else spectrumAmount_ = clampv(a, 0, 560);
            applySpectrumExtension();
            return 0;

        case kEqEnable: stLegacyEq_.target = on; return 0;
        case kEqBand:
            if (count < 2 || a < 0 || a > 9) return -EINVAL;
            legacyEqDb_[static_cast<size_t>(a)] = clampv(v[1] / 100.0f, -15.0f, 15.0f);
            applyLegacyEq();
            return 0;

        case kFieldSurroundEnable: stField_.target = on; return 0;
        case kFieldSurroundMidImage:
        case kFieldSurroundDepth:
            if (id == kFieldSurroundDepth) fieldDepth_ = clampv(a, 200, 800);
            else fieldMid_ = clampv(a, 100, 200);
            field_.set((fieldDepth_ - 200) / 600.0f, (fieldMid_ - 100) / 100.0f);
            return 0;

        case kDiffSurroundEnable: stHaas_.target = on; return 0;
        case kDiffSurroundDelay: haasMs_ = clampv(a, 100, 2000) / 100.0f; haas_.setDelayMs(haasMs_); return 0;

        case kReverbEnable: stReverb_.target = on; return 0;
        case kReverbRoomSize: reverb_p_.size = clampv(a, 0, 100); applyReverb(); return 0;
        case kReverbWidth: reverb_p_.width = clampv(a, 0, 100); applyReverb(); return 0;
        case kReverbDamping: reverb_p_.damping = clampv(a, 0, 100); applyReverb(); return 0;
        case kReverbWet: reverb_p_.wet = clampv(a, 0, 100); applyReverb(); return 0;
        case kReverbDry: reverb_p_.dry = clampv(a, 0, 100); applyReverb(); return 0;

        case kAgcEnable: stAgc_.target = on; return 0;
        case kAgcRatio: agc_p_.ratio = a; applyAgc(); return 0;
        case kAgcVolume: agc_p_.volume = clampv(a, 10, 100); applyAgc(); return 0;
        case kAgcMaxScaler: agc_p_.maxScaler = clampv(a, 100, 3000); applyAgc(); return 0;

        case kDynamicSystemEnable: stDyn_.target = on; return 0;
        case kDynamicSystemX:
        case kDynamicSystemY:
        case kDynamicSystemSide:
        case kDynamicSystemStrength: {
            fx::DynamicBass::Settings& ds = dynSettings_;  // accumulates the pairs as they arrive
            if (id == kDynamicSystemStrength) {
                ds.strength = clampv((a - 100) / 1000.0f, 0.0f, 1.0f);
            } else {
                if (count < 2) return -EINVAL;
                if (id == kDynamicSystemX) {
                    ds.lowMidHz = static_cast<float>(v[0]);
                    ds.presenceHz = static_cast<float>(v[1]);
                } else if (id == kDynamicSystemY) {
                    ds.bassQ = v[0] / 100.0f;
                    ds.bassHz = static_cast<float>(v[1]);
                } else {
                    ds.lowMidTrim = v[0] / 100.0f;
                    ds.presenceTrim = v[1] / 100.0f;
                }
            }
            dyn_.set(ds);
            return 0;
        }

        case kBassEnable: stBass_.target = on; return 0;
        case kBassMode: bass_p_.mode = clampv(a, 0, 2); applyBass(); return 0;
        case kBassFreq: bass_p_.freq = clampv(a, 15, 300); applyBass(); return 0;
        case kBassGain: bass_p_.gain = clampv(a, 50, 600); applyBass(); return 0;

        case kClarityEnable: stClarity_.target = on; return 0;
        case kClarityMode: clarity_p_.mode = clampv(a, 0, 2); applyClarity(); return 0;
        case kClarityGain: clarity_p_.gain = clampv(a, 0, 450); applyClarity(); return 0;

        case kCrossfeedEnable: stCure_.target = on; return 0;
        case kCrossfeedLevel: {
            // Bauer stereophonic-to-binaural presets (cutoff Hz, feed dB) as popularised by bs2b,
            // ordered mild -> strong: Jan Meier, Chu Moy, default (a higher feed level means less
            // crossfeed in bs2b terms).
            static constexpr float kCut[3] = {650.0f, 700.0f, 700.0f};
            static constexpr float kFeed[3] = {9.5f, 6.0f, 4.5f};
            const int l = clampv(a, 0, 2);
            cureSettings_.mode = 0;
            cureSettings_.cutoffHz = kCut[l];
            cureSettings_.feedDb = kFeed[l];
            cure_.set(cureSettings_);
            return 0;
        }

        case kTubeEnable: stTube_.target = on; return 0;
        case kAnalogEnable: stAnalog_.target = on; return 0;
        case kAnalogLevel: analogLevel_ = clampv(a, 0, 2); analog_.setLevel(analogLevel_); return 0;

        case kOutputVolume: targetGain_ = clampv(a, 1, 400) / 100.0f; return 0;
        case kChannelPan: pan_ = clampv(a, -100, 100); return 0;
        case kLimiterCeiling: {
            limiterCeilingDb_ = std::min(linearX100ToDb(clampv(a, 10, 100)), -0.1f);
            Limiter::Settings ls;
            ls.ceilingDb = limiterCeilingDb_;
            limiter_.set(ls);
            return 0;
        }

        case kSpeakerOptimization: stSpeaker_.target = on; return 0;

        case kCompEnable: stComp_.target = on; return 0;
        case kCompThreshold: comp_p_.threshold = clampv(a, 0, 100); applyComp(); return 0;
        case kCompRatio: comp_p_.ratio = clampv(a, 0, 100); applyComp(); return 0;
        case kCompKnee: comp_p_.knee = clampv(a, 0, 100); applyComp(); return 0;
        case kCompAutoKnee: comp_p_.autoKnee = on; applyComp(); return 0;
        case kCompGain: comp_p_.gain = clampv(a, 0, 100); applyComp(); return 0;
        case kCompAutoGain: comp_p_.autoGain = on; applyComp(); return 0;
        case kCompAttack: comp_p_.attack = clampv(a, 1, 500); applyComp(); return 0;
        case kCompAutoAttack: comp_p_.autoAttack = on; applyComp(); return 0;
        case kCompRelease: comp_p_.release = clampv(a, 1, 2000); applyComp(); return 0;
        case kCompAutoRelease: comp_p_.autoRelease = on; applyComp(); return 0;
        case kCompKneeMulti: comp_p_.kneeMulti = clampv(a, 0, 100); applyComp(); return 0;
        case kCompMaxAttack: comp_p_.maxAttack = clampv(a, 1, 500); applyComp(); return 0;
        case kCompMaxRelease: comp_p_.maxRelease = clampv(a, 1, 2000); applyComp(); return 0;
        case kCompCrest: comp_p_.crest = clampv(a, 0, 200); return 0;
        case kCompAdapt: comp_p_.adapt = clampv(a, 0, 100); return 0;
        case kCompNoClip: comp_p_.noClip = on; return 0;

        // ---- extensions ----
        case kAutoEqEnable: stAutoEq_.target = on; return 0;
        case kUserEqEnable: stUserEq_.target = on; return 0;

        case kLoudnessEnable: stLoud_.target = on; return 0;
        case kLoudnessReference: loud_p_.reference = clampv(a, 40, 100); applyLoudness(); return 0;
        case kLoudnessStrength: loud_p_.strength = clampv(a, 0, 100); applyLoudness(); return 0;
        case kLoudnessVolume: loud_p_.volumeX100 = clampv(a, -12000, 0); applyLoudness(); return 0;
        case kLoudnessMode: loud_p_.mode = clampv(a, 0, 1); applyLoudness(); return 0;
        case kLoudnessManual: loud_p_.manual = clampv(a, 20, 100); applyLoudness(); return 0;
        case kLoudnessCalibration: loud_p_.calibration = clampv(a, 60, 120); applyLoudness(); return 0;

        case kGraphicEqEnable: stGeq_.target = on; return 0;
        case kGraphicEqLayout: geqLayout_ = clampv(a, 0, 2); applyGraphicEq(); return 0;

        case kMultibandEnable: stMbc_.target = on; return 0;
        case kDynamicEqEnable: stDeq_.target = on; return 0;

        case kDeEsserEnable: stDeess_.target = on; return 0;
        case kDeEsserFreq:
        case kDeEsserThreshold:
        case kDeEsserAmount:
        case kDeEsserMode: {
            DeEsser::Settings& ds = deessSettings_;
            if (id == kDeEsserFreq) ds.freq = static_cast<float>(clampv(a, 2000, 16000));
            else if (id == kDeEsserThreshold) ds.thresholdDb = clampv(a / 100.0f, -60.0f, 0.0f);
            else if (id == kDeEsserAmount) ds.amountDb = clampv(a / 100.0f, 0.0f, 24.0f);
            else ds.mode = clampv(a, 0, 1);
            deess_.set(ds);
            return 0;
        }

        case kVocalEnable: stVocal_.target = on; return 0;
        case kVocalCentre:
        case kVocalAmbience:
        case kVocalLow:
        case kVocalHigh:
        case kVocalFocus: {
            CentreExtractor::Settings& cs = vocalSettings_;
            if (id == kVocalCentre) cs.centreDb = clampv(a / 100.0f, -40.0f, 12.0f);
            else if (id == kVocalAmbience) cs.ambienceDb = clampv(a / 100.0f, -40.0f, 12.0f);
            else if (id == kVocalLow) cs.lowHz = static_cast<float>(clampv(a, 20, 2000));
            else if (id == kVocalHigh) cs.highHz = static_cast<float>(clampv(a, 1000, 20000));
            else cs.focus = clampv(a / 100.0f, 0.0f, 1.0f);
            vocal_.set(cs);
            return 0;
        }

        case kSpatialEnable: spatial_p_.spatial = on; applySpatial(); return 0;
        case kSpatialAngle: spatial_p_.angle = clampv(a, 10, 90); applySpatial(); return 0;
        case kSpatialRoom: spatial_p_.room = clampv(a, 0, 100); applySpatial(); return 0;
        case kSpatialYaw: spatial_p_.yawX100 = clampv(a, -18000, 18000); applySpatial(); return 0;
        case kSpatialTracking: spatial_p_.tracking = on; applySpatial(); return 0;

        default:
            return -EINVAL;
    }
}

int Engine::setPayload(int32_t id, const uint8_t* value, uint32_t size) {
    // Every payload starts with a uint32 element count followed by floats (the convolution
    // chunk has its buffer id first).
    if (size < 8 || size % 4 != 0) return -EINVAL;
    const uint32_t floatsAvailable = (size - 4) / 4;
    uint32_t count;
    std::memcpy(&count, value, 4);
    auto floats = [&](uint32_t offsetBytes, uint32_t n) {
        std::vector<float> out(n);
        std::memcpy(out.data(), value + offsetBytes, static_cast<size_t>(n) * 4);
        for (float& f : out) if (!std::isfinite(f)) f = 0.0f;
        return out;
    };

    switch (id) {
        case kDdcCoefficients: {
            if (count == 0) { ddc_.setCoefficients(0, nullptr, nullptr); return 0; }
            if (static_cast<uint64_t>(count) * 2 > floatsAvailable) return -EINVAL;
            const auto c = floats(4, count * 2);
            return ddc_.setCoefficients(count, c.data(), c.data() + count) ? 0 : -EINVAL;
        }
        case kConvSetBuffer: {
            // [bufferId][floatCount][floats]
            uint32_t n;
            std::memcpy(&n, value + 4, 4);
            if (n > (size - 8) / 4) return -EINVAL;
            if (pendingBufferId_ == 0 || count != pendingBufferId_) return -EINVAL;
            if (pendingKernel_.size() + n > kMaxKernelFloats) {
                pendingBufferId_ = 0;
                pendingKernel_.clear();
                return -EINVAL;
            }
            const size_t at = pendingKernel_.size();
            pendingKernel_.resize(at + n);
            std::memcpy(pendingKernel_.data() + at, value + 8, static_cast<size_t>(n) * 4);
            return 0;
        }
        case kAutoEqFilters:
        case kUserEqFilters: {
            const uint32_t maxBands = 32;
            if (count > maxBands || 1 + static_cast<uint64_t>(count) * 4 > floatsAvailable) return -EINVAL;
            const auto f = floats(4, 1 + count * 4);
            const float preamp = clampv(f[0], -60.0f, 30.0f);
            std::vector<EqBand> bands;
            const int maxType = id == kAutoEqFilters ? 2 : static_cast<int>(FilterType::Count) - 1;
            for (uint32_t i = 0; i < count; ++i) {
                const float* b = &f[1 + i * 4];
                const int type = static_cast<int>(b[0]);
                // Invalid entries are dropped rather than rejecting the whole profile.
                if (type < 0 || type > maxType || b[1] <= 0.0f || b[3] <= 0.0f || std::fabs(b[2]) > 40.0f) continue;
                EqBand e;
                e.enabled = true;
                e.type = static_cast<FilterType>(type);
                e.freq = clampv(b[1], 10.0f, static_cast<float>(0.49 * fs_));
                e.gainDb = b[2];
                e.q = clampv(b[3], 0.05f, 40.0f);
                e.channel = 0;
                // At or above Nyquist a band can't be realised: skip it.
                if (b[1] >= 0.4999f * static_cast<float>(fs_)) continue;
                bands.push_back(e);
            }
            ParametricEq& eq = id == kAutoEqFilters ? autoEq_ : userEq_;
            eq.setBands(bands.data(), static_cast<int>(bands.size()));
            eq.setPreampDb(preamp);
            return 0;
        }
        case kGraphicEqGains: {
            if (count > 31 || count > floatsAvailable) return -EINVAL;
            geqDb_ = floats(4, count);
            applyGraphicEq();
            return 0;
        }
        case kMultibandSettings: {
            constexpr uint32_t kFloats = 4 + 4 + 5 * 5;
            if (count < kFloats || kFloats > floatsAvailable) return -EINVAL;
            const auto f = floats(4, kFloats);
            MultibandCompressor::Settings s;
            s.bands = clampv(static_cast<int>(f[0]), 3, 5);
            s.kneeDb = clampv(f[1], 0.0f, 24.0f);
            s.mix = clampv(f[2], 0.0f, 1.0f);
            s.autoMakeup = f[3] != 0.0f;
            for (int i = 0; i < 4; ++i) s.crossovers[i] = clampv<double>(f[4 + i], 40.0, 16000.0);
            for (int i = 0; i < 5; ++i) {
                s.threshold[i] = clampv(f[8 + i], -60.0f, 0.0f);
                s.ratio[i] = clampv(f[13 + i], 1.0f, 20.0f);
                s.attackMs[i] = clampv(f[18 + i], 0.1f, 500.0f);
                s.releaseMs[i] = clampv(f[23 + i], 5.0f, 3000.0f);
                s.gainDb[i] = clampv(f[28 + i], -24.0f, 24.0f);
            }
            mbc_.set(s);
            return 0;
        }
        case kDynamicEqBands: {
            constexpr uint32_t kPer = 10;
            if (count > 8 || static_cast<uint64_t>(count) * kPer > floatsAvailable) return -EINVAL;
            const auto f = floats(4, count * kPer);
            DynamicBand bands[8];
            for (uint32_t i = 0; i < count; ++i) {
                const float* b = &f[i * kPer];
                DynamicBand& d = bands[i];
                d.enabled = b[0] != 0.0f;
                d.type = clampv(static_cast<int>(b[1]), 0, 2);
                d.freq = clampv(b[2], 20.0f, 20000.0f);
                d.q = clampv(b[3], 0.1f, 10.0f);
                d.thresholdDb = clampv(b[4], -80.0f, 0.0f);
                d.rangeDb = clampv(b[5], -24.0f, 24.0f);
                d.ratio = clampv(b[6], 1.0f, 20.0f);
                d.attackMs = clampv(b[7], 0.1f, 500.0f);
                d.releaseMs = clampv(b[8], 5.0f, 3000.0f);
                d.mode = clampv(static_cast<int>(b[9]), 0, 1);
            }
            deq_.setBands(bands, static_cast<int>(count));
            return 0;
        }
        default:
            return -EINVAL;
    }
}

bool Engine::commitKernel(uint32_t bufferId, uint32_t crc, uint32_t kernelId) {
    std::vector<float> data;
    data.swap(pendingKernel_);
    const int paths = pendingPaths_;
    const bool sameUpload = bufferId != 0 && bufferId == pendingBufferId_;
    pendingBufferId_ = 0;
    if (!sameUpload || data.empty() || (paths != 1 && paths != 2 && paths != 4)) return false;
    const uint32_t actual = crc32(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(float));
    if (actual != crc) return false;
    if (actual == kernelCrc_ && kernel_) {
        kernelId_ = kernelId;  // same impulse response re-sent: adopt the new id
        return true;
    }
    const int length = static_cast<int>(data.size() / static_cast<size_t>(paths));
    if (length < 16) return false;

    // Deinterleave into one buffer per path.
    std::vector<std::vector<float>> path(static_cast<size_t>(paths), std::vector<float>(static_cast<size_t>(length)));
    for (int i = 0; i < length; ++i)
        for (int p = 0; p < paths; ++p) path[static_cast<size_t>(p)][static_cast<size_t>(i)] = data[static_cast<size_t>(i) * paths + p];
    std::vector<const float*> ptrs;
    for (auto& p : path) ptrs.push_back(p.data());

    auto built = ConvKernel::build(ptrs.data(), paths, length, kConvBlock);
    if (!built) return false;
    if (built->partitions > conv_.maxPartitions()) {
        conv_.setKernel(nullptr);
        conv_.prepare(kConvBlock, built->partitions);
    }
    // The audio path crossfades from the old kernel, so keep it alive until the next change.
    retiredKernel_ = std::move(kernel_);
    kernel_ = std::move(built);
    conv_.setKernel(kernel_.get());
    kernelSamples_ = std::move(data);
    kernelPaths_ = paths;
    kernelCrc_ = actual;
    kernelId_ = kernelId;
    return true;
}

// ---- Derived settings -----------------------------------------------------------------------------

void Engine::applyBass() {
    BassEnhancer::Settings s;
    // Natural: a clean dynamic shelf. Pure bass+: the same boost plus upper harmonics so it also
    // carries on small drivers. Subwoofer: a lower, deeper shelf plus a sub-octave.
    s.mode = bass_p_.mode == 1 ? BassEnhancer::kHybrid : BassEnhancer::kShelf;
    s.freq = static_cast<float>(bass_p_.mode == 2 ? std::max(20, bass_p_.freq * 4 / 5) : bass_p_.freq);
    s.gainDb = tableDb(kBassBoostDb, 12, bass_p_.gain, 50, 50) + (bass_p_.mode == 2 ? 4.0f : 0.0f);
    s.harmonics = bass_p_.mode == 1 ? 0.25f : 0.0f;
    s.protect = 0.6f;
    bass_.set(s);
    bass_p_.subwoofer = bass_p_.mode == 2;
    SubHarmonic::Settings ss;
    ss.freq = std::min(static_cast<float>(bass_p_.freq), 120.0f);
    ss.amount = clampv(s.gainDb / 17.0f, 0.0f, 1.0f) * 0.5f;
    sub_.set(ss);
}

void Engine::applyClarity() {
    // The gain is what the slider shows: treble lift in dB. Each mode places it differently and
    // adds a different amount of harmonic detail.
    const float g = tableDb(kClarityBoostDb, 10, clarity_p_.gain, 0, 50);
    Clarity::Settings s;
    EqBand b[2];
    int nb = 1;
    switch (clarity_p_.mode) {
        case 0:  // natural: a broad presence-to-air shelf
            s.mode = 0; s.freq = 5000.0f; s.amount = 0.05f * g / 14.8f;
            b[0] = {true, FilterType::HighShelf, 4000.0f, g, 0.6f, 0};
            break;
        case 1:  // exciter (OZone+): sparkle concentrated in the top octaves
            s.mode = 0; s.freq = 4000.0f; s.amount = 0.35f * g / 14.8f;
            b[0] = {true, FilterType::HighShelf, 8000.0f, g, 0.7071f, 0};
            break;
        default:  // hi-fi (XHiFi): wide lift from the upper mids, regenerated top octave
            s.mode = 1; s.freq = 7000.0f; s.amount = 0.4f * g / 14.8f;
            b[0] = {true, FilterType::HighShelf, 600.0f, 0.6f * g, 0.5f, 0};
            b[1] = {true, FilterType::HighShelf, 6000.0f, 0.4f * g, 0.7071f, 0};
            nb = 2;
            break;
    }
    s.airDb = 0.0f;
    clarity_.set(s);
    clarityEq_.setBands(b, nb);
}

void Engine::applyComp() {
    Compressor::Settings s;
    s.thresholdDb = -60.0f + 0.6f * static_cast<float>(comp_p_.threshold);
    // Ratio 0..100 % on a musical curve: 50 % = 3.5:1, 100 % = 8:1 (beyond that is limiting).
    s.ratio = 1.0f + 7.0f * std::pow(comp_p_.ratio / 100.0f, 1.5f);
    s.kneeDb = comp_p_.autoKnee ? clampv(6.0f + 12.0f * comp_p_.kneeMulti / 100.0f, 0.0f, 24.0f)
                                : 24.0f * comp_p_.knee / 100.0f;
    s.attackMs = static_cast<float>(comp_p_.autoAttack ? compAutoAttack_ : comp_p_.attack);
    s.releaseMs = static_cast<float>(comp_p_.autoRelease ? compAutoRelease_ : comp_p_.release);
    // Auto gain restores a third of the gain removed at full scale: louder and denser, without
    // dragging up quiet passages and noise as full compensation would.
    s.autoMakeup = false;
    s.makeupDb = comp_p_.autoGain ? 0.35f * -s.thresholdDb * (1.0f - 1.0f / s.ratio) : 24.0f * comp_p_.gain / 100.0f;
    s.mix = 1.0f;
    s.scHpfHz = 40.0f;
    comp_.set(s);
    SoftClipper::Settings cs;
    cs.thresholdDb = -0.5f;
    cs.softness = 0.6f;
    compClip_.set(cs);
}

void Engine::applyAgc() {
    Agc::Settings s;
    s.targetLufs = -14.0f + linearX100ToDb(agc_p_.volume);
    s.maxGainDb = linearX100ToDb(agc_p_.maxScaler);
    s.speed = agc_p_.ratio <= 50 ? 0.25f : agc_p_.ratio <= 100 ? 0.5f : 0.85f;
    agc_.set(s);
}

void Engine::applyReverb() {
    Reverb::Settings s;
    const float size = reverb_p_.size / 100.0f;
    s.size = 0.15f + 0.85f * size;
    s.decayS = 0.4f + 3.6f * size;
    s.damping = reverb_p_.damping / 100.0f;
    s.width = reverb_p_.width / 100.0f;
    s.predelayMs = 5.0f + 25.0f * size;
    s.early = 0.5f;
    s.wetDb = reverb_p_.wet <= 0 ? -120.0f : -30.0f + 30.0f * reverb_p_.wet / 100.0f;
    s.dryDb = static_cast<float>(gainToDb(std::max(reverb_p_.dry, 1) / 50.0));
    reverb_.set(s);
}

void Engine::applyLoudness() {
    double listen = loud_p_.mode == 0 ? loud_p_.calibration + loud_p_.volumeX100 / 100.0
                                      : static_cast<double>(loud_p_.manual);
    listen = clampv(listen, 20.0, 100.0);
    EqBand bands[16];
    const int n = designLoudnessCompensation(listen, loud_p_.reference, loud_p_.strength / 100.0, fs_, bands, 16);
    loud_.setBands(bands, n);
}

void Engine::applyLegacyEq() {
    EqBand bands[31];
    const int n = designGraphicEq(0, legacyEqDb_.data(), 10, fs_, bands, 31);
    legacyEq_.setBands(bands, n);
}

void Engine::applyGraphicEq() {
    EqBand bands[31];
    const int n = designGraphicEq(geqLayout_, geqDb_.data(), static_cast<int>(geqDb_.size()), fs_, bands, 31);
    geq_.setBands(bands, n);
}

void Engine::applySpatial() {
    // Headphone surround levels 1-5 are virtual-speaker presets; the spatial-audio section
    // exposes the same renderer directly (and head tracking).
    static constexpr float kAngle[5] = {22.0f, 30.0f, 38.0f, 45.0f, 55.0f};
    static constexpr float kRoom[5] = {0.10f, 0.20f, 0.30f, 0.40f, 0.55f};
    HrtfSpatializer::Settings s;
    if (spatial_p_.spatial) {
        s.angleDeg = static_cast<float>(spatial_p_.angle);
        s.room = spatial_p_.room / 100.0f;
        s.tracking = spatial_p_.tracking;
        s.yawDeg = spatial_p_.tracking ? spatial_p_.yawX100 / 100.0f : 0.0f;
    } else {
        s.angleDeg = kAngle[spatial_p_.vheLevel];
        s.room = kRoom[spatial_p_.vheLevel];
        s.tracking = false;
        s.yawDeg = 0.0f;
    }
    hrtf_.set(s);
    updateHrtfCompensation(s);
    stHrtf_.target = spatial_p_.vhe || spatial_p_.spatial;
}

void Engine::applySpectrumExtension() {
    // Regenerates content above the reference frequency and restores the top octave's level.
    const float amount = spectrumAmount_ / 560.0f;
    vse_.set({1, static_cast<float>(spectrumRef_), amount, 0.0f});
    EqBand b{true, FilterType::HighShelf, static_cast<float>(std::min<double>(spectrumRef_, 0.4 * fs_)), 4.0f * amount,
             0.7071f, 0};
    vseEq_.setBands(&b, 1);
}

void Engine::updateHrtfCompensation(const HrtfSpatializer::Settings& s) {
    // The measured head responses colour the sound (pinna peaks and notches). Measure the
    // renderer's actual frontal response for these settings and fit an inverse graphic EQ, so the
    // virtual speakers sound like the speakers rather than the dummy head's ears. Head yaw
    // changes don't need a new fit.
    const double key = s.angleDeg * 1000.0 + s.room * 10.0 + fs_ * 1e6;
    if (key == hrtfCompKey_) return;
    hrtfCompKey_ = key;

    HrtfSpatializer probe;
    probe.prepare(fs_);
    HrtfSpatializer::Settings ps = s;
    ps.yawDeg = 0.0f;
    ps.tracking = false;
    probe.set(ps);
    const int n = 8192;
    std::vector<float> l(n, 0.0f), r(n, 0.0f);
    l[0] = r[0] = 1.0f;
    probe.process(l.data(), r.data(), n);

    const int count = geq::bandCount(geq::kThird31);
    const double* centres = geq::centers(geq::kThird31);
    std::vector<double> target(static_cast<size_t>(count), 0.0), gains(static_cast<size_t>(count), 0.0);
    for (int k = 0; k < count; ++k) {
        if (centres[k] >= 0.45 * fs_) continue;
        // Magnitude around the band centre, averaged over both ears.
        double power = 0.0;
        for (double f : {centres[k] * 0.89, centres[k], centres[k] * 1.12}) {
            const double w = 2.0 * kPi * f / fs_;
            for (const std::vector<float>* x : {&l, &r}) {
                double re = 0.0, im = 0.0;
                for (int i = 0; i < n; ++i) {
                    re += (*x)[static_cast<size_t>(i)] * std::cos(w * i);
                    im -= (*x)[static_cast<size_t>(i)] * std::sin(w * i);
                }
                power += re * re + im * im;
            }
        }
        const double db = powerToDb(power / 6.0);
        // Correct most of the colouration, but never boost a deep notch by more than 6 dB.
        target[static_cast<size_t>(k)] = clampv(-0.8 * db, -9.0, 6.0);
    }
    fitPeakingGains(centres, target.data(), count, geq::bandQ(geq::kThird31), fs_, gains.data(), 12.0);
    EqBand bands[31];
    const int used = std::min(count, 31);
    for (int k = 0; k < used; ++k) {
        const float g = static_cast<float>(gains[static_cast<size_t>(k)]);
        bands[k] = {std::fabs(g) > 0.05f, FilterType::Peak, static_cast<float>(centres[k]), g,
                    static_cast<float>(geq::bandQ(geq::kThird31)), 0};
    }
    hrtfEq_.setBands(bands, used);
}

// ---- Processing ------------------------------------------------------------------------------------

template <class Fn, class Reset>
void Engine::run(Stage& st, float* l, float* r, int n, Fn&& fn, Reset&& reset) {
    if (!st.target && st.mix <= 0.0f) return;
    if (st.target && st.mix >= 1.0f) {
        fn(l, r, n);
        return;
    }
    std::memcpy(dryL_.data(), l, static_cast<size_t>(n) * sizeof(float));
    std::memcpy(dryR_.data(), r, static_cast<size_t>(n) * sizeof(float));
    fn(l, r, n);
    const float step = st.target ? stageStep_ : -stageStep_;
    float mix = st.mix;
    for (int i = 0; i < n; ++i) {
        mix = clampv(mix + step, 0.0f, 1.0f);
        l[i] = dryL_[static_cast<size_t>(i)] + (l[i] - dryL_[static_cast<size_t>(i)]) * mix;
        r[i] = dryR_[static_cast<size_t>(i)] + (r[i] - dryR_[static_cast<size_t>(i)]) * mix;
    }
    st.mix = mix;
    if (!st.target && st.mix <= 0.0f) reset();  // start clean next time it is enabled
}

void Engine::process(float* left, float* right, int frames) {
    ScopedNoDenormals noDenormals;
    lastProcessMs_.store(nowMs(), std::memory_order_relaxed);
    for (int done = 0; done < frames; done += kMaxBlock) {
        const int n = std::min(kMaxBlock, frames - done);
        processBlock(left + done, right + done, n);
    }
}

void Engine::processBlock(float* l, float* r, int n) {
    run(stDdc_, l, r, n, [&](float* a, float* b, int m) { ddc_.process(a, b, m); }, [&] { ddc_.reset(); });
    run(stAutoEq_, l, r, n, [&](float* a, float* b, int m) { autoEq_.process(a, b, m); }, [&] { autoEq_.reset(); });
    run(stConv_, l, r, n,
        [&](float* a, float* b, int m) {
            conv_.process(a, b, m);
            if (crossChannel_ > 0) {
                const float x = crossChannel_ / 200.0f;  // up to an equal blend at 100 %
                for (int i = 0; i < m; ++i) {
                    const float la = a[i], rb = b[i];
                    a[i] = la * (1.0f - x) + rb * x;
                    b[i] = rb * (1.0f - x) + la * x;
                }
            }
        },
        [&] { conv_.reset(); });
    run(stUserEq_, l, r, n, [&](float* a, float* b, int m) { userEq_.process(a, b, m); }, [&] { userEq_.reset(); });
    run(stLegacyEq_, l, r, n, [&](float* a, float* b, int m) { legacyEq_.process(a, b, m); }, [&] { legacyEq_.reset(); });
    run(stGeq_, l, r, n, [&](float* a, float* b, int m) { geq_.process(a, b, m); }, [&] { geq_.reset(); });
    run(stLoud_, l, r, n, [&](float* a, float* b, int m) { loud_.process(a, b, m); }, [&] { loud_.reset(); });
    run(stDeq_, l, r, n, [&](float* a, float* b, int m) { deq_.process(a, b, m); }, [&] { deq_.reset(); });
    run(stDeess_, l, r, n, [&](float* a, float* b, int m) { deess_.process(a, b, m); }, [&] { deess_.reset(); });
    run(stVocal_, l, r, n, [&](float* a, float* b, int m) { vocal_.process(a, b, m); }, [&] { vocal_.reset(); });
    run(stVse_, l, r, n,
        [&](float* a, float* b, int m) {
            vse_.process(a, b, m);
            vseEq_.process(a, b, m);
        },
        [&] { vse_.reset(); vseEq_.reset(); });
    run(stBass_, l, r, n,
        [&](float* a, float* b, int m) {
            bass_.process(a, b, m);
            if (bass_p_.subwoofer) sub_.process(a, b, m);
        },
        [&] { bass_.reset(); sub_.reset(); });
    run(stDyn_, l, r, n, [&](float* a, float* b, int m) { dyn_.process(a, b, m); }, [&] { dyn_.reset(); });
    run(stClarity_, l, r, n,
        [&](float* a, float* b, int m) {
            clarityEq_.process(a, b, m);
            clarity_.process(a, b, m);
        },
        [&] { clarity_.reset(); clarityEq_.reset(); });
    run(stTube_, l, r, n,
        [&](float* a, float* b, int m) {
            tube_.process(a, b, m);
            tubeEq_.process(a, b, m);
        },
        [&] { tube_.reset(); tubeEq_.reset(); });
    run(stAnalog_, l, r, n, [&](float* a, float* b, int m) { analog_.process(a, b, m); }, [&] { analog_.reset(); });
    run(stComp_, l, r, n,
        [&](float* a, float* b, int m) {
            if (comp_p_.autoAttack || comp_p_.autoRelease) {
                // Programme-dependent timing from the crest factor: transient material (high
                // crest) gets the fast attack/slow release end, dense material the opposite.
                double peak = 0.0, sum = 0.0;
                for (int i = 0; i < m; ++i) {
                    const double s = std::max(std::fabs(a[i]), std::fabs(b[i]));
                    peak = std::max(peak, s);
                    sum += s * s;
                }
                const double rms = std::sqrt(sum / std::max(m, 1));
                const double crestDb = rms > 1e-6 ? gainToDb(peak / rms) : 0.0;
                const double adapt = 0.02 + 0.3 * comp_p_.adapt / 100.0;
                compCrestEnv_ += adapt * (crestDb - compCrestEnv_);
                const double t = clampv((compCrestEnv_ - 3.0) / 15.0 * comp_p_.crest / 100.0, 0.0, 1.0);
                compAutoAttack_ = comp_p_.maxAttack + (comp_p_.attack - comp_p_.maxAttack) * t;
                compAutoRelease_ = comp_p_.release + (comp_p_.maxRelease - comp_p_.release) * t;
                if (++compCounter_ >= 4) {
                    compCounter_ = 0;
                    applyComp();
                }
            }
            comp_.process(a, b, m);
            if (comp_p_.noClip) compClip_.process(a, b, m);
        },
        [&] { comp_.reset(); compClip_.reset(); });
    run(stMbc_, l, r, n, [&](float* a, float* b, int m) { mbc_.process(a, b, m); }, [&] { mbc_.reset(); });
    run(stField_, l, r, n, [&](float* a, float* b, int m) { field_.process(a, b, m); }, [&] { field_.reset(); });
    run(stHaas_, l, r, n, [&](float* a, float* b, int m) { haas_.process(a, b, m); }, [&] { haas_.reset(); });
    run(stCure_, l, r, n, [&](float* a, float* b, int m) { cure_.process(a, b, m); }, [&] { cure_.reset(); });
    run(stHrtf_, l, r, n,
        [&](float* a, float* b, int m) {
            hrtf_.process(a, b, m);
            hrtfEq_.process(a, b, m);
        },
        [&] { hrtf_.reset(); hrtfEq_.reset(); });
    run(stReverb_, l, r, n, [&](float* a, float* b, int m) { reverb_.process(a, b, m); }, [&] { reverb_.reset(); });
    run(stSpeaker_, l, r, n, [&](float* a, float* b, int m) { speaker_.process(a, b, m); }, [&] { speaker_.reset(); });
    run(stAgc_, l, r, n, [&](float* a, float* b, int m) { agc_.process(a, b, m); }, [&] { agc_.reset(); });

    // Master: output gain and balance (ramped), then the true-peak limiter.
    const int ramp = static_cast<int>(0.02 * fs_);
    outGain_.setTarget(targetGain_, ramp);
    const float p = pan_ / 100.0f;
    panL_.setTarget(p > 0.0f ? 1.0f - p : 1.0f, ramp);
    panR_.setTarget(p < 0.0f ? 1.0f + p : 1.0f, ramp);
    for (int i = 0; i < n; ++i) {
        const float g = outGain_.next();
        l[i] *= g * panL_.next();
        r[i] *= g * panR_.next();
    }
    limiter_.process(l, r, n);
}

}  // namespace rfx
