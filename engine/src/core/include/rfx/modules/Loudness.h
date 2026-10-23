// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Psychoacoustic loudness tools:
//  - ISO 226:2023 normal equal-loudness-level contours
//  - equal-loudness (volume-dependent) compensation curves
//  - ITU-R BS.1770-4 K-weighted loudness meter (momentary / short-term LUFS)
//  - age / audiogram based hearing compensation targets (ISO 7029:2017)

#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/modules/Eq.h"

namespace rfx {

namespace iso226 {
constexpr int kCount = 29;
extern const double kFreq[kCount];
/// Sound pressure level (dB SPL) of a pure tone at table frequency index i perceived at
/// loudness level `phon` (ISO 226:2023 Formula 1).
double splAtIndex(int i, double phon);
/// SPL at an arbitrary frequency (log-frequency interpolation of the contour, 20 Hz..12.5 kHz,
/// held constant outside).
double spl(double freqHz, double phon);
/// Gain (dB) needed at `freqHz` so that a programme mixed at `referencePhon` keeps its
/// perceived spectral balance when reproduced at `listeningPhon`.  0 dB at 1 kHz.
double compensationDb(double freqHz, double listeningPhon, double referencePhon);
}  // namespace iso226

/// Builds the band list (octave peaking filters) realising the ISO 226 compensation curve.
int designLoudnessCompensation(double listeningPhon, double referencePhon, double strength, double sampleRate,
                               EqBand* out, int maxBands);

namespace iso7029 {
constexpr int kCount = 11;
extern const double kFreq[kCount];  // 125 .. 8000 Hz audiometric frequencies
/// Median hearing threshold deviation (dB) re age 18 for a population of the given sex
/// (0 = male, 1 = female, 2 = average of both).
double medianDeviation(int freqIndex, double ageYears, int sex);
}  // namespace iso7029

/// Hearing compensation for one ear: thresholds (dB HL) at the 11 audiometric frequencies.
/// Applies `strength` x the threshold elevation above 15 dB HL (strength 0.5 = the classic
/// half-gain rule), tapers gain below 500 Hz and above 6 kHz, caps at +20 dB, and fits it
/// with 1/3-octave peaking filters on the given channel.
int designHearingCompensation(const float* thresholdsDbHl, double strength, int channel, double sampleRate,
                              EqBand* out, int maxBands);
/// The target curve (dB) of the above at an arbitrary frequency (for plots and headroom).
double hearingCompensationDb(const float* thresholdsDbHl, double strength, double freqHz);

/// ITU-R BS.1770-4 loudness meter.  K-weighting is derived for any sample rate from the
/// analog prototype of the 48 kHz reference filters.
class LoudnessMeter {
public:
    void prepare(double sampleRate);
    void reset();
    void process(const float* left, const float* right, int n);
    double momentaryLufs() const;   // 400 ms window
    double shortTermLufs() const;   // 3 s window
    bool blockReady() { bool r = newBlock_; newBlock_ = false; return r; }  // a new 100 ms block arrived
    int blocks() const { return filled_; }  // 100 ms blocks measured since reset (saturates)

    static BiquadCoeffs kWeightingShelf(double sampleRate);
    static BiquadCoeffs kWeightingHighPass(double sampleRate);

private:
    BiquadCoeffs shelf_, hp_;
    BiquadState sL1_, sL2_, sR1_, sR2_;
    int blockLen_ = 4800, blockPos_ = 0;
    double blockSum_ = 0.0;
    static constexpr int kRing = 30;
    double ring_[kRing] = {};
    int ringPos_ = 0, filled_ = 0;
    bool newBlock_ = false;
};

}  // namespace rfx
