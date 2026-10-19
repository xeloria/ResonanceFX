// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// FIR design from an arbitrary magnitude response, and band-limited resampling.

#include <vector>

namespace rfx {

struct CurvePoint {
    double freq;
    double gainDb;
};

/// Designs an FIR whose magnitude follows `points` (interpolated linearly on a log-frequency
/// axis, held constant outside the given range).
///  - minimumPhase: real-cepstrum (homomorphic) minimum-phase reconstruction; no pre-ringing,
///    latency-free apart from the convolver block.
///  - otherwise linear phase, delay = (taps-1)/2 samples.
std::vector<float> designFirFromCurve(const std::vector<CurvePoint>& points, double sampleRate, int taps,
                                      bool minimumPhase);

/// Same as above, but from a magnitude sampled on a uniform grid of `bins` points 0..fs/2.
std::vector<float> designFirFromMagnitude(const std::vector<double>& magnitudeLinear, int taps,
                                          bool minimumPhase);

/// Kaiser-windowed sinc resampler (offline, used for impulse responses).
std::vector<float> resampleOffline(const float* in, int frames, int channels, double srcRate, double dstRate,
                                   int* outFrames);

/// Kaiser window value for position x in [-1, 1].
double kaiserWindow(double x, double beta);

/// Zeroth-order modified Bessel function of the first kind.
double besselI0(double x);

}  // namespace rfx
