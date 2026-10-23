// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Phase-coherent multiway Linkwitz-Riley crossover and the multiband compressor built on it.

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"

namespace rfx {

/// Splits a signal into up to 5 bands with 4th-order Linkwitz-Riley filters.  Lower bands are
/// passed through the all-pass equivalents of the higher crossovers, so the bands sum to an
/// all-pass response (flat magnitude) whatever their individual gains are set to 0 dB.
class LrCrossover {
public:
    static constexpr int kMaxBands = 5;
    void configure(const double* freqs, int crossovers, double sampleRate);
    void reset();
    int bands() const { return count_ + 1; }
    /// Splits one stereo sample into per-band samples.
    inline void split(double l, double r, double* bl, double* br);

private:
    struct Stage {
        BiquadCoeffs lp, hp, ap;
        BiquadState lpL[2], lpR[2], hpL[2], hpR[2];
    };
    Stage st_[kMaxBands - 1];
    // all-pass compensation state: [band][crossover]
    BiquadState apL_[kMaxBands][kMaxBands - 1], apR_[kMaxBands][kMaxBands - 1];
    int count_ = 0;
};

inline void LrCrossover::split(double l, double r, double* bl, double* br) {
    double restL = l, restR = r;
    for (int k = 0; k < count_; ++k) {
        Stage& s = st_[k];
        double lowL = biquadTick(s.lp, s.lpL[1], biquadTick(s.lp, s.lpL[0], restL));
        double lowR = biquadTick(s.lp, s.lpR[1], biquadTick(s.lp, s.lpR[0], restR));
        restL = biquadTick(s.hp, s.hpL[1], biquadTick(s.hp, s.hpL[0], restL));
        restR = biquadTick(s.hp, s.hpR[1], biquadTick(s.hp, s.hpR[0], restR));
        for (int j = k + 1; j < count_; ++j) {
            lowL = biquadTick(st_[j].ap, apL_[k][j], lowL);
            lowR = biquadTick(st_[j].ap, apR_[k][j], lowR);
        }
        bl[k] = lowL;
        br[k] = lowR;
    }
    bl[count_] = restL;
    br[count_] = restR;
}

/// Multiband compressor: one soft-knee, log-domain smooth-decoupled compressor per band
/// (Giannoulis, Massberg & Reiss 2012) on a phase-coherent crossover.  `mix` blends each band
/// with its uncompressed self, so parallel compression stays phase aligned.
class MultibandCompressor {
public:
    struct Settings {
        int bands = 3;
        double crossovers[4] = {150, 2500, 6000, 12000};
        float threshold[5] = {-24, -24, -24, -24, -24};
        float ratio[5] = {3, 2.5f, 2, 2, 2};
        float attackMs[5] = {30, 15, 8, 5, 3};
        float releaseMs[5] = {300, 200, 150, 120, 100};
        float gainDb[5] = {0, 0, 0, 0, 0};
        float kneeDb = 6.0f, mix = 1.0f;
        bool autoMakeup = true;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    float gainReductionDb() const { return grDb_; }
    /// Gain already applied before the engine (dB, <= 0): the system volume on outputs where
    /// Android scales the mix before effects. Levels are judged on the programme itself, so the
    /// volume knob doesn't move the threshold.
    void setLevelOffsetDb(float db) { offsetDb_ = db; }

private:
    double gainComputer(int b, double xDb) const;
    double offsetDb_ = 0.0;
    Settings s_;
    double fs_ = 48000.0;
    LrCrossover xo_;
    double aA_[5] = {}, aR_[5] = {}, y1_[5] = {}, yL_[5] = {}, makeup_[5] = {1, 1, 1, 1, 1};
    float grDb_ = 0.0f;
};

}  // namespace rfx
