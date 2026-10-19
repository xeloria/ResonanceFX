// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Second-order IIR sections: coefficient design (RBJ cookbook and analog-matched)
// and a transposed direct form II processor running in double precision.

#include <complex>

namespace rfx {

/// Filter types.  The numeric values are part of the parameter protocol (PEQ band encoding),
/// the preset format and the Equalizer APO importer, so never renumber them.
enum class FilterType : int {
    Peak = 0,
    LowShelf = 1,
    HighShelf = 2,
    LowPass = 3,
    HighPass = 4,
    BandPass = 5,
    Notch = 6,
    AllPass = 7,
    LowShelf1 = 8,   // first order, 6 dB/oct
    HighShelf1 = 9,  // first order, 6 dB/oct
    LowPass1 = 10,   // first order
    HighPass1 = 11,  // first order
    Tilt = 12,       // first-order tilt: -gain/2 at DC, +gain/2 at Nyquist, pivot at f0
    Count = 13
};

inline bool filterHasGain(FilterType t) {
    return t == FilterType::Peak || t == FilterType::LowShelf || t == FilterType::HighShelf ||
           t == FilterType::LowShelf1 || t == FilterType::HighShelf1 || t == FilterType::Tilt;
}

struct BiquadCoeffs {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    /// Complex response at normalised angular frequency w (radians/sample).
    std::complex<double> response(double w) const;
    double magnitudeDb(double freqHz, double sampleRate) const;
    bool isStable() const;
    static BiquadCoeffs identity() { return {}; }
};

/// RBJ "Audio EQ Cookbook" bilinear designs (Equalizer APO / AutoEQ compatible).
BiquadCoeffs designRbj(FilterType type, double f0, double gainDb, double q, double sampleRate);

/// Analog-matched designs: poles by impulse invariance, zeros chosen so the magnitude
/// matches the analog prototype exactly at DC, at f0 and at Nyquist (after Vicanek, 2016).
/// Avoids the response "cramping" of bilinear filters near Nyquist.  First-order types fall
/// back to the bilinear design.
BiquadCoeffs designMatched(FilterType type, double f0, double gainDb, double q, double sampleRate);

inline BiquadCoeffs designFilter(FilterType type, double f0, double gainDb, double q,
                                 double sampleRate, bool matched) {
    return matched ? designMatched(type, f0, gainDb, q, sampleRate)
                   : designRbj(type, f0, gainDb, q, sampleRate);
}

/// Magnitude of the continuous-time prototype (the "ideal" response the designs approximate).
double analogMagnitudeDb(FilterType type, double f0, double gainDb, double q, double freqHz);

/// Q values of the second-order sections of an Nth-order Butterworth filter.
/// Returns the number of second-order sections written; `hasFirstOrder` is set for odd N.
int butterworthQs(int order, double* qs, bool* hasFirstOrder);

struct BiquadState {
    double s1 = 0.0, s2 = 0.0;
    void reset() { s1 = s2 = 0.0; }
};

/// Transposed direct form II, one sample.
inline double biquadTick(const BiquadCoeffs& c, BiquadState& s, double x) {
    const double y = c.b0 * x + s.s1;
    s.s1 = c.b1 * x - c.a1 * y + s.s2;
    s.s2 = c.b2 * x - c.a2 * y;
    return y;
}

/// Convenience single-channel biquad.
struct Biquad {
    BiquadCoeffs c;
    BiquadState s;
    inline float process(float x) { return static_cast<float>(biquadTick(c, s, x)); }
    void processBlock(float* x, int n) {
        for (int i = 0; i < n; ++i) x[i] = static_cast<float>(biquadTick(c, s, x[i]));
    }
    void reset() { s.reset(); }
};

}  // namespace rfx
