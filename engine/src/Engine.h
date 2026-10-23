// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "fx/Fx.h"
#include "rfx/dsp/Convolver.h"
#include "rfx/modules/Binaural.h"
#include "rfx/modules/Character.h"
#include "rfx/modules/DynamicEq.h"
#include "rfx/modules/Dynamics.h"
#include "rfx/modules/Enhance.h"
#include "rfx/modules/Eq.h"
#include "rfx/modules/Multiband.h"
#include "rfx/modules/Spatial.h"
#include "rfx/modules/Speaker.h"
#include "rfx/modules/Spectral.h"

namespace rfx {

/// The ResonanceFX DSP chain. Processes planar stereo; the Android glue (EffectContext) handles
/// sample formats, channel layouts and threading. Not thread safe: the caller serialises
/// setParameter/prepare against process.
class Engine {
public:
    static constexpr int kMaxBlock = 4096;

    Engine();

    void prepare(double sampleRate);
    /// Clears signal state (filters, delay lines, envelopes). Android sends disable/config/enable
    /// around every seek and track change on some outputs, so by default what the playback gain
    /// control has learned is kept; `full` (a new sample rate) forgets that too.
    void reset(bool full = false);
    /// value/size: the parameter's value bytes as received (int32 scalars or a fixed payload).
    /// Returns 0 or -EINVAL.
    int setParameter(int32_t id, const uint8_t* value, uint32_t size);
    void process(float* left, float* right, int frames);

    double sampleRate() const { return fs_; }
    uint32_t kernelId() const { return kernelId_; }
    uint64_t lastProcessMs() const { return lastProcessMs_.load(std::memory_order_relaxed); }

private:
    // Crossfades a module in and out over ~20 ms so switching never clicks.
    struct Stage {
        bool target = false;
        float mix = 0.0f;
    };
    template <class Fn, class Reset>
    void run(Stage& st, float* l, float* r, int n, Fn&& fn, Reset&& reset);
    void processBlock(float* l, float* r, int n);

    int setInts(int32_t id, const int32_t* v, int count);
    int setPayload(int32_t id, const uint8_t* value, uint32_t size);

    void applyBass();
    void applyClarity();
    void applyComp();
    void applyAgc();
    void applyReverb();
    void applyLoudness();
    void applyLegacyEq();
    void applyGraphicEq();
    void applySpatial();
    void applySpectrumExtension();
    void updateHrtfCompensation(const HrtfSpatializer::Settings& s);
    bool commitKernel(uint32_t bufferId, uint32_t crc, uint32_t kernelId);

    double fs_ = 48000.0;
    float stageStep_ = 0.0f;
    std::vector<float> dryL_, dryR_;
    std::atomic<uint64_t> lastProcessMs_{0};
#ifdef RFX_DIAG
    void diagnostics(const float* l, const float* r, int n);
    double diagSum_ = 0.0, diagPeak_ = 0.0;
    int diagFrames_ = 0;
#endif

    // ---- modules, in processing order ----
    Stage stDdc_, stAutoEq_, stConv_, stUserEq_, stLegacyEq_, stGeq_, stLoud_, stDeq_, stDeess_, stVocal_,
        stVse_, stBass_, stDyn_, stClarity_, stTube_, stAnalog_, stComp_, stMbc_, stField_, stHaas_, stCure_,
        stHrtf_, stReverb_, stXtc_, stSpeaker_, stAgc_, stProtect_;

    fx::BiquadCascade ddc_;
    ParametricEq autoEq_{32}, userEq_{32}, legacyEq_{31}, geq_{31}, loud_{64};
    // Tone shaping that gives clarity, spectrum extension and tube their voicing, and the
    // compensation that keeps the virtual-speaker renderer tonally neutral.
    ParametricEq clarityEq_{2}, vseEq_{1}, tubeEq_{1}, hrtfEq_{31};
    double hrtfCompKey_ = -1.0;
    PartitionedConvolver conv_;
    DynamicEq deq_;
    DeEsser deess_;
    CentreExtractor vocal_;
    Clarity vse_, clarity_;
    BassEnhancer bass_;
    SubHarmonic sub_;
    fx::DynamicBass dyn_;
    Tube tube_;
    fx::AnalogColor analog_;
    Compressor comp_;
    SoftClipper compClip_;
    MultibandCompressor mbc_;
    fx::FieldSurround field_;
    fx::HaasDelay haas_;
    Crossfeed cure_;
    HrtfSpatializer hrtf_;
    Reverb reverb_;
    CrosstalkCanceller xtc_;
    fx::SpeakerVoicing speaker_;
    Agc agc_;
    SpeakerProtector protect_;
    Limiter limiter_;

    // ---- parameter state (protocol units) ----
    struct {
        int mode = 0, freq = 70, gain = 50;
        bool subwoofer = false;
    } bass_p_;
    struct {
        int mode = 0, gain = 50;
    } clarity_p_;
    struct {
        int threshold = 100, ratio = 100, knee = 0, kneeMulti = 0, gain = 0, attack = 20, maxAttack = 80;
        int release = 50, maxRelease = 100, crest = 100, adapt = 50;
        bool autoKnee = true, autoGain = true, autoAttack = true, autoRelease = false, noClip = true;
    } comp_p_;
    double compCrestEnv_ = 0.0, compPeakEnv_ = 0.0, compAutoAttack_ = 20.0, compAutoRelease_ = 50.0;
    int compCounter_ = 0;
    struct {
        int ratio = 50, volume = 100, maxScaler = 300;
    } agc_p_;
    struct {
        int size = 0, width = 0, damping = 0, wet = 0, dry = 50;
    } reverb_p_;
    struct {
        int reference = 80, strength = 100, volumeX100 = 0, mode = 0, manual = 60, calibration = 100;
    } loud_p_;
    std::array<float, 10> legacyEqDb_{};
    int geqLayout_ = 0;
    std::vector<float> geqDb_;
    struct {
        bool vhe = false, spatial = false, tracking = false;
        int vheLevel = 0, angle = 30, room = 30, yawX100 = 0;
    } spatial_p_;
    fx::DynamicBass::Settings dynSettings_;
    DeEsser::Settings deessSettings_;
    CentreExtractor::Settings vocalSettings_;
    Crossfeed::Settings cureSettings_;
    float limiterCeilingDb_ = -0.1f;
    float haasMs_ = 5.0f;
    int analogLevel_ = 0;
    int spectrumRef_ = 7600, spectrumAmount_ = 0;
    int fieldDepth_ = 200, fieldMid_ = 150;
    int crossChannel_ = 0;
    int speakerProfile_ = 0, speakerStrength_ = 100;
    SpeakerProtector::Settings protectSettings_;
    CrosstalkCanceller::Settings xtcSettings_;

    // ---- master ----
    LinearRamp outGain_, panL_, panR_;
    float targetGain_ = 1.0f;
    float levelOffsetDb_ = 0.0f;
    int agcMaxCut_ = 0;
    int pan_ = 0;

    // ---- convolution kernel upload ----
    std::vector<float> pendingKernel_;
    uint32_t pendingBufferId_ = 0;
    int pendingPaths_ = 0;
    std::unique_ptr<ConvKernel> kernel_, retiredKernel_;
    uint32_t kernelId_ = 0, kernelCrc_ = 0;
    std::vector<float> kernelSamples_;  // last committed (interleaved) for re-preparing
    int kernelPaths_ = 0;
};

}  // namespace rfx
