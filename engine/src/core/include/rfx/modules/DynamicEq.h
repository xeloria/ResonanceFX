// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Dynamic EQ (level-dependent filter gains) and a de-esser.

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"

namespace rfx {

struct DynamicBand {
    bool enabled = false;
    int type = 0;            // 0 bell, 1 low shelf, 2 high shelf
    float freq = 1000.0f, q = 1.0f;
    float thresholdDb = -30.0f;
    float rangeDb = -6.0f;   // maximum gain change; negative cuts, positive boosts
    float ratio = 3.0f;
    float attackMs = 5.0f, releaseMs = 120.0f;
    int mode = 0;            // 0 = act above threshold, 1 = act below threshold
};

int decodeDynamicBands(const float* v, int count, DynamicBand* out, int maxBands);

/// Each band watches its own frequency region through a side-chain filter (band-pass for bells,
/// low/high-pass for shelves) and moves its filter gain towards `rangeDb` as the side-chain
/// level crosses the threshold (soft knee, ratio-controlled slope, attack/release smoothing).
/// Unlike static EQ it only intervenes when needed - e.g. taming a resonance that is harsh only
/// on loud notes, or lifting bass only in quiet passages.
class DynamicEq {
public:
    static constexpr int kMaxBands = 8;
    void prepare(double sampleRate);
    void reset();
    void setBands(const DynamicBand* bands, int n);
    void process(float* left, float* right, int n);
    float activityDb() const { return activity_; }  // largest current gain change

private:
    struct Slot {
        DynamicBand b;
        BiquadCoeffs sc, main;
        BiquadState scS, mL, mR;
        double env = -120.0, aA = 0.0, aR = 0.0;
        float applied = 0.0f;
    };
    void configure(Slot& s);
    Slot slots_[kMaxBands];
    int count_ = 0;
    double fs_ = 48000.0;
    float activity_ = 0.0f;
};

/// De-esser.  The side-chain is the high-passed (sibilance) band; its level above the threshold
/// is reduced 4:1 up to `amount` dB with millisecond attack and short release.  Split-band mode
/// applies the reduction through a high shelf so only the sibilant range is lowered; wide-band
/// mode ducks the whole signal (more natural on some voices).
class DeEsser {
public:
    struct Settings {
        float freq = 6500.0f, thresholdDb = -28.0f, amountDb = 8.0f;
        int mode = 0;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    float reductionDb() const { return reduction_; }

private:
    Settings s_;
    double fs_ = 48000.0;
    BiquadCoeffs sc_, shelf_;
    BiquadState sc1_, sc2_, shL_, shR_;
    double env_ = -120.0, aA_ = 0.0, aR_ = 0.0;
    float applied_ = 0.0f, reduction_ = 0.0f;
    int counter_ = 0;
};

}  // namespace rfx
