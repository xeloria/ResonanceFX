// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Character effects: sub-harmonic synthesiser and a soft clipper.

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"
#include "rfx/dsp/Oversampler.h"

namespace rfx {

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

}  // namespace rfx
