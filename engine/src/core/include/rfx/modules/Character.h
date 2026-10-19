// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Character and repair effects: transient shaper, sub-harmonic synthesiser, soft clipper,
// tape/vinyl emulation, declicker and per-channel delay.

#include <cstdint>
#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"
#include "rfx/dsp/Oversampler.h"

namespace rfx {

/// Level-independent transient shaper (differential envelope principle, as in the SPL
/// Transient Designer): two followers with equal release but different attack times differ
/// only during onsets; two with equal attack but different release times differ only during
/// decays.  Each difference (in dB) drives its own gain, so the effect does not depend on level.
class TransientShaper {
public:
    struct Settings {
        float attackDb = 0.0f, sustainDb = 0.0f, speed = 0.5f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);

private:
    Settings s_;
    double fs_ = 48000.0;
    double aFast_ = 0, aSlow_ = 0, rel_ = 0, rFast_ = 0, rSlow_ = 0, gainC_ = 0;
    double e1_ = 0, e2_ = 0, e3_ = 0, e4_ = 0, gainDb_ = 0.0;
};

/// Octave-divider sub-harmonic synthesiser (the dbx 100/120 principle): the low band drives a
/// flip-flop that toggles on every positive-going zero crossing, producing a square wave at half
/// the frequency whose amplitude follows the band envelope; low-pass filtering leaves a clean tone
/// one octave below the bass line.
class SubHarmonic {
public:
    struct Settings {
        float amount = 0.4f, freq = 100.0f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);

private:
    Settings s_;
    double fs_ = 48000.0;
    BiquadCoeffs hp_, lp_, out_, dc_;
    BiquadState hpS_, lp1S_, lp2S_, o1S_, o2S_, o3S_, dcS_;
    double env_ = 0.0, envRel_ = 0.0, prev_ = 0.0;
    double ff_ = 1.0;
    bool armed_ = true;
};

/// Soft clipper with a quadratic knee (continuous slope), 2x oversampled.  Shaving the
/// sharpest peaks before the limiter lets the limiter work less, which keeps it from pumping.
class SoftClipper {
public:
    struct Settings {
        float thresholdDb = -2.0f, softness = 0.5f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    int latency() const { return Halfband2x::kLatency; }

private:
    inline float clip(float x) const;
    Settings s_;
    float t_ = 0.8f, k_ = 0.25f;
    Halfband2x upL_, upR_, dnL_, dnR_;
};

/// Analogue-medium emulation.
///  - Tape: saturation, head bump, gap-loss high-frequency roll-off, wow (0.5 Hz) and flutter
///    (~6 Hz) as a modulated delay, hiss.
///  - Vinyl: wow at 33 1/3 rpm (0.555 Hz), surface noise, crackle (Poisson-timed resonant
///    impulses), rumble and high-frequency loss with age.
class TapeVinyl {
public:
    struct Settings {
        int mode = 0;
        float drive = 0.3f, wow = 0.2f, flutter = 0.2f, noise = 0.1f, age = 0.3f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    /// Mean delay of the wow/flutter line (samples).
    int latency() const;

private:
    inline float rnd() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 8) * (1.0f / 8388608.0f) - 1.0f;  // [-1, 1)
    }
    Settings s_;
    double fs_ = 48000.0;
    std::vector<float> dl_, dr_;
    int mask_ = 0, wpos_ = 0;
    double phWow_ = 0, phFlutter_ = 0, drift_ = 0;
    BiquadCoeffs bump_, loss_, hissF_, crackF_, rumbleF_, dcF_;
    BiquadState bumpL_, bumpR_, lossL1_, lossL2_, lossR1_, lossR2_, hissL_, hissR_, crackS_, rumbleS_, dcL_, dcR_;
    float driveK_ = 1.0f, driveNorm_ = 1.0f, hissGain_ = 0.0f, crackRate_ = 0.0f, crackGain_ = 0.0f, rumbleGain_ = 0.0f;
    float crackEnv_ = 0.0f;
    uint32_t rng_ = 0xC0FFEEu;
};

/// Click and crackle remover.  Impulsive disturbances are detected as outliers of the
/// second-difference signal against a running robust scale, then the affected samples are
/// replaced by cubic Hermite interpolation from the surrounding clean samples (fixed look-ahead).
class Declicker {
public:
    struct Settings {
        float sensitivity = 0.5f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s) { s_ = s; }
    void process(float* left, float* right, int n);
    int latency() const { return kDelay; }
    int repairs() const { return repairs_; }

private:
    static constexpr int kSize = 512;  // ring buffer (power of two)
    static constexpr int kDelay = 96;  // output delay = look-ahead
    static constexpr int kMaxClick = 48;
    struct Channel {
        float buf[kSize] = {};
        double scale = 1e-4;
        int64_t clickStart = -1;
        int clickLen = 0, quiet = 0;
    };
    void run(Channel& c, float x, int64_t t);
    void repair(Channel& c, int64_t start, int len);
    Settings s_;
    Channel ch_[2];
    int64_t t_ = 0;
    double scaleCoef_ = 0.999;
    int repairs_ = 0;
};

/// Independent integer-sample delay per channel (speaker / car time alignment), with a
/// short crossfade when the delay changes.
class ChannelDelay {
public:
    void prepare(double sampleRate);
    void reset();
    void set(float leftMs, float rightMs);
    void process(float* left, float* right, int n);
    bool active() const { return dL_ > 0 || dR_ > 0 || fade_ > 0; }

private:
    double fs_ = 48000.0;
    std::vector<float> bl_, br_;
    int mask_ = 0, pos_ = 0;
    int dL_ = 0, dR_ = 0, oldL_ = 0, oldR_ = 0;
    int fade_ = 0, fadeLen_ = 480;
};

}  // namespace rfx
