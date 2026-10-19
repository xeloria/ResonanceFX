// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Loudness.h"

#include <cmath>

#include "rfx/dsp/Common.h"

namespace rfx {

// ---------------------------------------------------------------------------
// ISO 226:2023, Table 1 and Formula (1).
// ---------------------------------------------------------------------------
namespace iso226 {

const double kFreq[kCount] = {20,   25,   31.5, 40,   50,   63,   80,   100,  125,   160,
                              200,  250,  315,  400,  500,  630,  800,  1000, 1250,  1600,
                              2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500};
static const double kAlpha[kCount] = {0.635, 0.602, 0.569, 0.537, 0.509, 0.482, 0.456, 0.433, 0.412, 0.391,
                                      0.373, 0.357, 0.343, 0.330, 0.320, 0.311, 0.303, 0.300, 0.295, 0.292,
                                      0.290, 0.290, 0.289, 0.289, 0.289, 0.293, 0.303, 0.323, 0.354};
static const double kLu[kCount] = {-31.5, -27.2, -23.1, -19.3, -16.1, -13.1, -10.4, -8.2, -6.3, -4.6,
                                   -3.2,  -2.1,  -1.2,  -0.5,  0.0,   0.4,   0.5,   0.0,  -2.7, -4.2,
                                   -1.2,  1.4,   2.3,   1.0,   -2.3,  -7.2,  -11.2, -10.9, -3.5};
static const double kTf[kCount] = {78.1, 68.7, 59.5, 51.1, 44.0, 37.5, 31.5, 26.5, 22.1, 17.9,
                                   14.4, 11.4, 8.6,  6.2,  4.4,  3.0,  2.2,  2.4,  3.5,  1.7,
                                   -1.3, -4.2, -6.0, -5.4, -1.5, 6.0,  12.6, 13.9, 12.3};

double splAtIndex(int i, double phon) {
    const double af = kAlpha[i], lu = kLu[i], tf = kTf[i];
    // alpha_r = 0.300, T_r = 2.4 dB, p0 = 20 uPa -> (p0^2)^(alpha_r - alpha_f) = (4e-10)^(0.3 - af)
    const double term = std::pow(4e-10, 0.3 - af) * (std::pow(10.0, 0.03 * phon) - std::pow(10.0, 0.072)) +
                        std::pow(10.0, af * (tf + lu) / 10.0);
    return 10.0 / af * std::log10(std::max(term, 1e-30)) - lu;
}

double spl(double freqHz, double phon) {
    double ys[kCount];
    for (int i = 0; i < kCount; ++i) ys[i] = splAtIndex(i, phon);
    return interpLogX(kFreq, ys, kCount, freqHz);
}

double compensationDb(double freqHz, double listeningPhon, double referencePhon) {
    const double l = clampv(listeningPhon, 20.0, 100.0);
    const double r = clampv(referencePhon, 20.0, 100.0);
    return (spl(freqHz, l) - l) - (spl(freqHz, r) - r);
}

}  // namespace iso226

int designLoudnessCompensation(double listeningPhon, double referencePhon, double strength, double fs,
                               EqBand* out, int maxBands) {
    const int n = geq::bandCount(geq::kOctave10);
    const double* c = geq::centers(geq::kOctave10);
    const double q = geq::bandQ(geq::kOctave10);
    double target[16], gains[16];
    const double listen = std::min(listeningPhon, referencePhon);  // compensate quieter playback only
    bool any = false;
    for (int i = 0; i < n; ++i) {
        target[i] = clampv(strength, 0.0, 1.0) * iso226::compensationDb(c[i], listen, referencePhon);
        target[i] = clampv(target[i], -12.0, 18.0);
        if (std::fabs(target[i]) > 0.05) any = true;
    }
    for (int i = 0; i < n; ++i) gains[i] = 0.0;
    if (any) fitPeakingGains(c, target, n, q, fs, gains);
    const int count = std::min(n, maxBands);
    for (int i = 0; i < count; ++i) {
        out[i].type = FilterType::Peak;
        out[i].freq = static_cast<float>(c[i]);
        out[i].q = static_cast<float>(q);
        out[i].gainDb = static_cast<float>(gains[i]);
        out[i].enabled = std::fabs(gains[i]) > 0.01;
        out[i].channel = 0;
    }
    return count;
}

// ---------------------------------------------------------------------------
// ISO 7029:2017, Table 1: median threshold deviation = alpha * (Y - 18)^beta
// ---------------------------------------------------------------------------
namespace iso7029 {

const double kFreq[kCount] = {125, 250, 500, 750, 1000, 1500, 2000, 3000, 4000, 6000, 8000};
static const double kAlphaM[kCount] = {2.50e-6, 1.39e-4, 4.59e-4, 5.70e-4, 7.02e-4, 1.09e-3,
                                       1.56e-3, 2.54e-3, 3.40e-3, 4.53e-3, 5.06e-3};
static const double kAlphaF[kCount] = {6.16e-4, 3.98e-4, 2.61e-4, 2.25e-4, 2.21e-4, 2.53e-4,
                                       3.12e-4, 4.88e-4, 7.37e-4, 1.47e-3, 2.53e-3};
static const double kBetaM[kCount] = {3.841, 2.832, 2.537, 2.512, 2.494, 2.446, 2.404, 2.350, 2.325, 2.315, 2.328};
static const double kBetaF[kCount] = {2.451, 2.568, 2.708, 2.775, 2.805, 2.813, 2.792, 2.728, 2.660, 2.539, 2.439};

double medianDeviation(int i, double age, int sex) {
    if (i < 0 || i >= kCount || age <= 18.0) return 0.0;
    const double y = std::min(age, 80.0) - 18.0;
    const double m = kAlphaM[i] * std::pow(y, kBetaM[i]);
    const double f = kAlphaF[i] * std::pow(y, kBetaF[i]);
    if (sex == 0) return m;
    if (sex == 1) return f;
    return 0.5 * (m + f);
}

}  // namespace iso7029

double hearingCompensationDb(const float* thr, double strength, double f) {
    double ys[iso7029::kCount];
    for (int i = 0; i < iso7029::kCount; ++i) ys[i] = clampv<double>(thr[i], -20.0, 90.0);
    const double h = interpLogX(iso7029::kFreq, ys, iso7029::kCount, f);
    double g = clampv(strength, 0.0, 1.0) * std::max(0.0, h - 15.0);
    // Low-frequency taper: gain below 500 Hz rises from 50 % (125 Hz) to 100 % (500 Hz).
    // Speech prescriptions (NAL-R) cut LF gain much harder to limit upward spread of masking;
    // for music a gentler taper preserves bass balance.
    if (f < 500.0) {
        const double t = clampv(std::log(f / 125.0) / std::log(4.0), 0.0, 1.0);
        g *= 0.5 + 0.5 * t;
    }
    // High-frequency taper above 6 kHz: music listeners prefer less HF gain than speech fittings.
    if (f > 6000.0) {
        const double t = clampv(std::log(f / 6000.0) / std::log(16000.0 / 6000.0), 0.0, 1.0);
        g *= 1.0 - 0.4 * t;
    }
    return std::min(g, 20.0);
}

int designHearingCompensation(const float* thr, double strength, int channel, double fs, EqBand* out,
                              int maxBands) {
    const int n = geq::bandCount(geq::kThird31);
    const double* c = geq::centers(geq::kThird31);
    const double q = geq::bandQ(geq::kThird31);
    double target[31], gains[31];
    bool any = false;
    for (int i = 0; i < n; ++i) {
        target[i] = hearingCompensationDb(thr, strength, c[i]);
        gains[i] = 0.0;
        if (target[i] > 0.05) any = true;
    }
    if (any) fitPeakingGains(c, target, n, q, fs, gains);
    const int count = std::min(n, maxBands);
    for (int i = 0; i < count; ++i) {
        out[i].type = FilterType::Peak;
        out[i].freq = static_cast<float>(c[i]);
        out[i].q = static_cast<float>(q);
        out[i].gainDb = static_cast<float>(gains[i]);
        out[i].enabled = std::fabs(gains[i]) > 0.01;
        out[i].channel = channel;
    }
    return count;
}

// ---------------------------------------------------------------------------
// ITU-R BS.1770-4
// ---------------------------------------------------------------------------
BiquadCoeffs LoudnessMeter::kWeightingShelf(double fs) {
    const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    const double K = std::tan(kPi * f0 / fs);
    const double Vh = std::pow(10.0, G / 20.0);
    const double Vb = std::pow(Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;
    BiquadCoeffs c;
    c.b0 = (Vh + Vb * K / Q + K * K) / a0;
    c.b1 = 2.0 * (K * K - Vh) / a0;
    c.b2 = (Vh - Vb * K / Q + K * K) / a0;
    c.a1 = 2.0 * (K * K - 1.0) / a0;
    c.a2 = (1.0 - K / Q + K * K) / a0;
    return c;
}

BiquadCoeffs LoudnessMeter::kWeightingHighPass(double fs) {
    const double f0 = 38.13547087602444, Q = 0.5003270373238773;
    const double K = std::tan(kPi * f0 / fs);
    const double a0 = 1.0 + K / Q + K * K;
    BiquadCoeffs c;
    c.b0 = 1.0;
    c.b1 = -2.0;
    c.b2 = 1.0;
    c.a1 = 2.0 * (K * K - 1.0) / a0;
    c.a2 = (1.0 - K / Q + K * K) / a0;
    return c;
}

void LoudnessMeter::prepare(double fs) {
    shelf_ = kWeightingShelf(fs);
    hp_ = kWeightingHighPass(fs);
    blockLen_ = std::max(1, static_cast<int>(std::lround(0.1 * fs)));
    reset();
}

void LoudnessMeter::reset() {
    sL1_.reset();
    sL2_.reset();
    sR1_.reset();
    sR2_.reset();
    blockPos_ = 0;
    blockSum_ = 0.0;
    for (double& v : ring_) v = 0.0;
    ringPos_ = 0;
    filled_ = 0;
    newBlock_ = false;
}

void LoudnessMeter::process(const float* left, const float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const double l = biquadTick(hp_, sL2_, biquadTick(shelf_, sL1_, left[i]));
        const double r = biquadTick(hp_, sR2_, biquadTick(shelf_, sR1_, right[i]));
        blockSum_ += l * l + r * r;
        if (++blockPos_ >= blockLen_) {
            ring_[ringPos_] = blockSum_ / blockLen_;
            ringPos_ = (ringPos_ + 1) % kRing;
            if (filled_ < kRing) ++filled_;
            blockSum_ = 0.0;
            blockPos_ = 0;
            newBlock_ = true;
        }
    }
}

static double meanOfLast(const double* ring, int ringSize, int pos, int filled, int count) {
    count = std::min(count, filled);
    if (count == 0) return 0.0;
    double s = 0.0;
    for (int k = 1; k <= count; ++k) s += ring[(pos - k + ringSize) % ringSize];
    return s / count;
}

double LoudnessMeter::momentaryLufs() const {
    return -0.691 + powerToDb(meanOfLast(ring_, kRing, ringPos_, filled_, 4), -150.0);
}

double LoudnessMeter::shortTermLufs() const {
    return -0.691 + powerToDb(meanOfLast(ring_, kRing, ringPos_, filled_, 30), -150.0);
}

}  // namespace rfx
