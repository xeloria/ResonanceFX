// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

// ResonanceFX-specific processors that complement the shared DSP core (engine/src/core).
// All process planar stereo buffers in place.

#include <array>
#include <cstdint>
#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/modules/Eq.h"

namespace rfx::fx {

/// Headphone correction from .vdc profiles: a cascade of biquads given at 44.1 and 48 kHz.
/// Other output rates get the 48 kHz set re-targeted through its analog prototype
/// (inverse bilinear transform, then bilinear at the new rate), so a profile still applies at
/// 88.2/96/192 kHz instead of silently switching off.
class BiquadCascade {
public:
    static constexpr int kMaxSections = 32;

    void prepare(double sampleRate);
    void reset();
    /// count = coefficients per rate (5 per section). Returns false if malformed.
    bool setCoefficients(uint32_t count, const float* at44k1, const float* at48k);
    void process(float* left, float* right, int n);

private:
    struct Section {
        double b0, b1, b2, a1, a2;  // y = b0x + b1x1 + b2x2 - a1y1 - a2y2
    };
    void rebuild();

    double fs_ = 48000.0;
    std::vector<Section> src44_, src48_, active_;
    std::vector<std::array<double, 8>> state_;
};

/// Mid/side image control: side level sets the surround strength, mid level the centre image.
class FieldSurround {
public:
    void prepare(double sampleRate);
    void reset();
    void set(float strength /*0..1*/, float midLevel /*0..1, 0.5 = unity*/);
    void process(float* left, float* right, int n);

private:
    double fs_ = 48000.0;
    float sideGain_ = 1.0f, midGain_ = 1.0f;
    // The widened side signal is high-passed so bass stays centred and mono-compatible.
    BiquadState hpL_, hpR_;
    BiquadCoeffs hp_;
};

/// Mid/side delay widener: delays the side signal by 1-20 ms, widening the image while the
/// centre stays put. The side band is high-passed so bass stays centred and mono-compatible.
class StereoWidener {
public:
    void prepare(double sampleRate);
    void reset();

    /// Sets the delay time in milliseconds (1-20 ms).
    void setDelayMs(float ms);

    /// Sets the gain multiplier for the delayed side signal (0.0 to 2.0).
    void setWidth(float width);

    /// Sets the high-pass cutoff frequency for the side signal (80-250 Hz) to protect bass.
    void setHighpassHz(float hz);

    /// Enables or disables the dynamic mono bypass protection.
    void setMonoProtection(bool enabled);

    void process(float* left, float* right, int n);

private:
    double fs_ = 48000.0;
    std::vector<float> buf_;
    int pos_ = 0, delay_ = 0;
    // Delay jumps would click, so the change lands in the middle of a short dip envelope.
    float pendingDelayMs_ = -1.0f;

    // Target delay in milliseconds, preserved across sample rate changes.
    float targetDelayMs_ = 5.0f;

    int dipRemaining_ = 0, dipTotal_ = 0;

    // Width parameters.
    float width_ = 1.0f;          // target width from the UI
    float widthCurrent_ = 1.0f;   // smoothed width used in the DSP
    float widthStep_ = 0.0f;      // constant step per sample to reach the target
    int widthRampRemaining_ = 0;  // samples remaining in the ramp

    float hpHz_ = 120.0f;
    bool monoProtectEnabled_ = true;

    // 24 dB/oct high-pass: two cascaded second-order sections.
    BiquadCoeffs hpCoeffs1_, hpCoeffs2_;
    BiquadState hpState1_, hpState2_;

    double lfoPhase_ = 0.0;
    double lfoInc_ = 0.0;

    // Envelope followers for dynamic mono protection.
    float midEnv_ = 0.0f;
    float sideEnv_ = 0.0f;
    float envCoef_ = 0.0f;
    int envCounter_ = 0;
    bool isMonoActive_ = false;
    float monoFactor_ = 1.0f;
};

/// Precedence-effect (Haas) spaciousness: delays the right channel by 1-20 ms, so the ear hears
/// one source pulled towards the earlier side. Fully wet by default, as the original implementation
/// was: that is the classic setup and the only setting that keeps the right channel's level flat,
/// but the direct sound then exists on one side only, which on closely spaced speakers reads as a
/// hard shift, and the mono sum cancels where the comb nulls fall (100, 300 Hz...). The mix control
/// is there to soften either.
class HaasDelay {
public:
    void prepare(double sampleRate);
    void reset();

    /// Sets the delay time in milliseconds (1-20 ms).
    void setDelayMs(float ms);

    /// Sets the wet/dry mix (0 = right channel untouched, 1 = fully delayed).
    void setMix(float mix);

    void process(float* left, float* right, int n);

private:
    /// Samples for `ms`, clamped so the read tap always stays behind the write pointer.
    int delaySamples(float ms) const;
    double fs_ = 48000.0;
    std::vector<float> buf_;
    // `targetDelayMs_` and `pendingDelayMs_` are milliseconds, the unit the UI and the presets
    // use; `delay_` and `newDelay_` are sample counts at the current rate.
    int pos_ = 0, delay_ = 0;
    // A delay change reads both taps and crossfades between them, so the delayed band never drops
    // out and the slider can be dragged without clicks. `newDelay_` is the tap being faded in.
    int newDelay_ = 0;
    int xfadeRemaining_ = 0, xfadeTotal_ = 0;
    float pendingDelayMs_ = -1.0f;

    // Target delay in milliseconds, preserved across sample rate changes.
    float targetDelayMs_ = 5.0f;

    // Wet/dry mix: fully wet by default, as the original implementation was. Lower values soften
    // the image shift and the mono comb, at the cost of a level that depends on frequency.
    float mix_ = 1.0f;           // target mix from the UI
    float mixCurrent_ = 1.0f;    // smoothed mix used in the DSP
    float mixStep_ = 0.0f;       // constant step per sample to reach the target
    int mixRampRemaining_ = 0;   // samples remaining in the ramp

    double lfoPhase_ = 0.0;
    double lfoInc_ = 0.0;
};

/// Dynamic bass: a low shelf whose boost rises when the programme is light on bass and backs
/// off when it is already bass heavy, with tonal-balance trims tuned per device type.
class DynamicBass {
public:
    struct Settings {
        float lowMidHz = 400.0f, presenceHz = 5600.0f;  // trim corners
        float bassQ = 0.5f, bassHz = 80.0f;
        float lowMidTrim = 0.5f, presenceTrim = 0.5f;   // 0..1.2
        float strength = 0.0f;                           // 0..1 -> up to 12 dB
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);

private:
    void updateShelf(double boostDb);
    Settings s_;
    double fs_ = 48000.0;
    BiquadCoeffs detLp_{}, detHp_{}, shelf_{}, lowMid_{}, presence_{};
    BiquadState detLpS_, detHpS_, shelfL_, shelfR_, lowMidL_, lowMidR_, presL_, presR_;
    double bassEnv_ = 0.0, fullEnv_ = 0.0, envCoef_ = 0.0, boostDb_ = 0.0, smoothCoef_ = 0.0;
    double appliedBoostDb_ = -1.0;
    int counter_ = 0;
};

/// Analog-console coloration: gentle asymmetric saturation with a warm tilt.
class AnalogColor {
public:
    void prepare(double sampleRate);
    void reset();
    void setLevel(int level /*0..2*/);
    void process(float* left, float* right, int n);

private:
    double fs_ = 48000.0;
    int level_ = 0;
    float drive_ = 0.15f;
    ParametricEq tone_{2};
};

/// Loudspeaker correction. Built-in profiles are typical corrections for a class of speaker
/// (each one an EQ curve at 100 % strength); `kMeasured` uses bands fitted to the user's own
/// measurement of their speaker. Strength scales every band's gain.
class SpeakerVoicing {
public:
    enum Profile { kPhone = 0, kTablet, kLaptop, kSmallBluetooth, kLargeBluetooth, kCar, kTv, kMeasured, kProfileCount };
    static constexpr int kMaxBands = 32;

    void prepare(double sampleRate);
    void reset();
    void setProfile(int profile, float strength /*0..1*/);
    /// The measured correction (only heard with kMeasured).
    void setMeasured(const EqBand* bands, int n, float preampDb);
    void process(float* left, float* right, int n);

private:
    void rebuild();
    double fs_ = 48000.0;
    int profile_ = kPhone;
    float strength_ = 1.0f;
    std::vector<EqBand> measured_;
    float measuredPreampDb_ = 0.0f;
    ParametricEq eq_{kMaxBands};
};

}  // namespace rfx::fx
