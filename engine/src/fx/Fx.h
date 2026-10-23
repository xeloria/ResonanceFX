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

/// Precedence-effect (Haas) spaciousness: delays the right channel by 1-20 ms.
class HaasDelay {
public:
    void prepare(double sampleRate);
    void reset();
    void setDelayMs(float ms);
    void process(float* left, float* right, int n);

private:
    double fs_ = 48000.0;
    std::vector<float> buf_;
    int pos_ = 0, delay_ = 0;
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
