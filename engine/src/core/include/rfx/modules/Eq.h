// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Parametric EQ with click-free parameter glides, plus the least-squares band-gain
// solver used by the graphic EQ, loudness compensation and hearing compensation.

#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"

namespace rfx {

struct EqBand {
    bool enabled = false;
    FilterType type = FilterType::Peak;
    float freq = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.7071f;
    int channel = 0;  // 0 = both, 1 = left, 2 = right, 3 = mid, 4 = side

    bool operator==(const EqBand& o) const {
        return enabled == o.enabled && type == o.type && freq == o.freq && gainDb == o.gainDb && q == o.q &&
               channel == o.channel;
    }
};

/// Decodes the protocol encoding (enabled, type, freq, gain, q, channel) * n.
int decodeBands(const float* values, int count, EqBand* out, int maxBands);

class ParametricEq {
public:
    explicit ParametricEq(int capacity = 32);

    void prepare(double sampleRate);
    void reset();  // clears state and jumps smoothed parameters to their targets
    int capacity() const { return static_cast<int>(slots_.size()); }

    /// Real-time safe.  Bands beyond `n` are faded out.
    void setBands(const EqBand* bands, int n);
    void setMatched(bool matched);
    void setPreampDb(float db);

    void process(float* left, float* right, int n);
    bool idle() const;  // nothing to do (all bands off, preamp 0 dB, no glides)

    /// Magnitude of the *target* configuration (control-thread helper).
    static double responseDb(const EqBand* bands, int n, bool matched, double freq, double sampleRate);

private:
    struct Slot {
        EqBand target;
        bool active = false;
        bool smoothing = false;
        bool fadingOut = false;
        float freq = 1000.0f, gain = 0.0f, q = 0.7071f;
        FilterType type = FilterType::Peak;
        int channel = 0;
        BiquadCoeffs c;
        BiquadState sL, sR;
    };
    void updateCoeffs(Slot& s);
    void stepSmoothing(Slot& s);

    std::vector<Slot> slots_;
    double fs_ = 48000.0;
    double smoothK_ = 0.1;
    bool matched_ = false;
    LinearRamp preamp_;
};

// ---------------------------------------------------------------------------
// Graphic EQ (accurate cascade design, after Valimaki & Liski 2016 / Liski 2017)
// ---------------------------------------------------------------------------
namespace geq {

enum Layout { kOctave10 = 0, kTwoThirds15 = 1, kThird31 = 2 };

int bandCount(int layout);
const double* centers(int layout);
double bandQ(int layout);

}  // namespace geq

/// Fits gains of peaking filters (fixed centres and Q) so that the cascade passes through
/// `targetDb` at the centres and approximately through the geometric midpoints.
/// Two-pass least squares with an interaction matrix re-estimated at the solved gains.
/// `outGains` receives one gain per centre (0 for centres above 0.45 fs).
void fitPeakingGains(const double* centersHz, const double* targetDb, int count, double q, double sampleRate,
                     double* outGains, double maxGainDb = 30.0, double centreWeight = 2.0, bool matched = true);

/// Builds the graphic-EQ band list for a layout and slider positions.
int designGraphicEq(int layout, const float* sliderDb, int sliders, double sampleRate, EqBand* out, int maxBands);

}  // namespace rfx
