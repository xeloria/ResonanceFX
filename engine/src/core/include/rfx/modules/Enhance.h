// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Enhancement stages: bass (dynamic shelf + psychoacoustic "missing fundamental"),
// clarity (harmonic exciter / bandwidth extension) and tube saturation.

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"
#include "rfx/dsp/Oversampler.h"

namespace rfx {

/// Bass enhancement.
///  - Dynamic shelf: low shelf whose boost backs off when the boosted bass would approach
///    full scale (protects small drivers and avoids limiter pumping).
///  - Psychoacoustic: Chebyshev-polynomial harmonic generator driven by the envelope-normalised
///    bass band.  For a sinusoid, T_k(cos t) = cos(k t), so exactly the 2nd..4th harmonics are
///    produced with a level tracking the fundamental (cf. MaxxBass, Ben-Tzur et al. 1999;
///    Larsen & Aarts 2004).  The auditory system infers the missing fundamental from the
///    harmonic series (residue pitch, Schouten 1940).
class BassEnhancer {
public:
    enum Mode { kShelf = 0, kPsychoacoustic = 1, kHybrid = 2 };
    struct Settings {
        int mode = kShelf;
        float freq = 100.0f, gainDb = 6.0f, harmonics = 0.5f, protect = 0.5f;
        bool replace = false;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    float protectionDb() const { return reductionDb_; }
    static double staticResponseDb(const Settings& s, double freq, double sampleRate);

private:
    void design();
    Settings s_;
    double fs_ = 48000.0;
    // shelf
    BiquadCoeffs shelf_;
    BiquadState shL_, shR_;
    float appliedGain_ = 0.0f;
    float reductionDb_ = 0.0f;
    BiquadCoeffs detLp_;
    BiquadState detS_;
    double detEnv_ = 0.0, detAtt_ = 0.0, detRel_ = 0.0;
    // harmonic generator
    BiquadCoeffs bpHp_, bpLp1_, bpLp2_, hHp_, hLp_;
    BiquadState bpHpS_, bpLp1S_, bpLp2S_, hHpS_, hLpS_, hHp2S_;
    double env_ = 0.0, envRel_ = 0.0;
    // replace (high-pass the original)
    BiquadCoeffs rHp_;
    BiquadState rL1_, rL2_, rR1_, rR2_;
    int counter_ = 0;
};

/// Clarity: band-limited harmonic exciter (2x oversampled) plus an "air" shelf.
///  - Exciter mode: harmonics of the band above `freq` are generated and added.
///  - Extension mode: harmonics of the octave below `freq` are generated and only content
///    above `freq` is kept - restores the band lost above a lossy codec's low-pass.
class Clarity {
public:
    struct Settings {
        int mode = 0;
        float freq = 3500.0f, amount = 0.3f, airDb = 0.0f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    int latency() const { return Halfband2x::kLatency; }
    static double staticResponseDb(const Settings& s, double freq, double sampleRate);

private:
    void design();
    Settings s_;
    double fs_ = 48000.0;
    BiquadCoeffs srcHp_, outHp_, air_;
    BiquadState srcL_, srcR_, outL_, outR_, out2L_, out2R_, airL_, airR_;
    Halfband2x upL_, upR_, dnL_, dnR_;
    FixedDelay<64> dryL_, dryR_;
    double envL_ = 0.0, envR_ = 0.0, envAtt_ = 0.0, envRel_ = 0.0;
};

/// Tube-style saturation: biased tanh transfer (even + odd harmonics) at 2x oversampling,
/// normalised to unity small-signal gain, DC-blocked, with latency-aligned dry/wet mix.
class Tube {
public:
    struct Settings {
        float drive = 0.3f, even = 0.6f, mix = 1.0f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    int latency() const { return Halfband2x::kLatency; }

private:
    inline float shape(float x) const;
    Settings s_;
    double fs_ = 48000.0;
    float k_ = 1.0f, bias_ = 0.0f, offset_ = 0.0f, norm_ = 1.0f;
    Halfband2x upL_, upR_, dnL_, dnR_;
    FixedDelay<64> dryL_, dryR_;
    BiquadCoeffs dc_;
    BiquadState dcL_, dcR_;
};

}  // namespace rfx
