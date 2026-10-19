// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Multiband.h"

#include <algorithm>
#include <cmath>

namespace rfx {

void LrCrossover::configure(const double* freqs, int crossovers, double fs) {
    count_ = clampv(crossovers, 0, kMaxBands - 1);
    double prev = 10.0;
    for (int k = 0; k < count_; ++k) {
        // Keep crossovers ascending, at least a third of an octave apart and below Nyquist.
        const double f = clampv(std::max(freqs[k], prev * 1.26), 20.0, 0.42 * fs);
        prev = f;
        st_[k].lp = designRbj(FilterType::LowPass, f, 0.0, 0.70710678, fs);
        st_[k].hp = designRbj(FilterType::HighPass, f, 0.0, 0.70710678, fs);
        // LR4 low-pass + high-pass = 2nd-order all-pass with Q = 1/sqrt(2).
        st_[k].ap = designRbj(FilterType::AllPass, f, 0.0, 0.70710678, fs);
    }
}

void LrCrossover::reset() {
    for (auto& s : st_) {
        for (int i = 0; i < 2; ++i) {
            s.lpL[i].reset();
            s.lpR[i].reset();
            s.hpL[i].reset();
            s.hpR[i].reset();
        }
    }
    for (int b = 0; b < kMaxBands; ++b)
        for (int k = 0; k < kMaxBands - 1; ++k) {
            apL_[b][k].reset();
            apR_[b][k].reset();
        }
}

void MultibandCompressor::prepare(double fs) {
    fs_ = fs;
    set(s_);
    xo_.configure(s_.crossovers, s_.bands - 1, fs_);  // rate changed: redesign unconditionally
    reset();
}

void MultibandCompressor::reset() {
    xo_.reset();
    for (int b = 0; b < 5; ++b) y1_[b] = yL_[b] = 0.0;
    grDb_ = 0.0f;
}

void MultibandCompressor::set(const Settings& s) {
    const bool reconfigure = s.bands != s_.bands ||
                             !std::equal(s.crossovers, s.crossovers + 4, s_.crossovers);
    s_ = s;
    s_.bands = clampv(s_.bands, 2, 5);
    for (int b = 0; b < 5; ++b) {
        s_.ratio[b] = clampv(s_.ratio[b], 1.0f, 50.0f);
        aA_[b] = onePoleCoef(clampv(s_.attackMs[b], 0.05f, 500.0f) * 1e-3, fs_);
        aR_[b] = onePoleCoef(clampv(s_.releaseMs[b], 5.0f, 5000.0f) * 1e-3, fs_);
        double m = s_.gainDb[b];
        if (s_.autoMakeup) m += -0.5 * gainComputer(b, 0.0);
        makeup_[b] = dbToGain(m);
    }
    if (reconfigure || xo_.bands() != s_.bands) {
        xo_.configure(s_.crossovers, s_.bands - 1, fs_);
        xo_.reset();
    }
}

double MultibandCompressor::gainComputer(int b, double x) const {
    const double T = s_.threshold[b], R = s_.ratio[b], W = s_.kneeDb;
    const double d = x - T;
    if (2.0 * d < -W) return x;
    if (W > 0.0 && 2.0 * std::fabs(d) <= W) {
        const double t = d + W / 2.0;
        return x + (1.0 / R - 1.0) * t * t / (2.0 * W);
    }
    return T + d / R;
}

void MultibandCompressor::process(float* left, float* right, int n) {
    const int nb = xo_.bands();
    const double mix = s_.mix;
    double bl[5], br[5];
    double maxGr = 0.0;
    for (int i = 0; i < n; ++i) {
        xo_.split(left[i], right[i], bl, br);
        double outL = 0.0, outR = 0.0;
        for (int b = 0; b < nb; ++b) {
            const double peak = std::max(std::fabs(bl[b]), std::fabs(br[b]));
            const double xDb = 20.0 * std::log10(std::max(peak, 1e-6));
            const double xL = xDb - gainComputer(b, xDb);
            y1_[b] = std::max(xL, aR_[b] * y1_[b] + (1.0 - aR_[b]) * xL);
            yL_[b] = aA_[b] * yL_[b] + (1.0 - aA_[b]) * y1_[b];
            maxGr = std::max(maxGr, yL_[b]);
            const double g = std::pow(10.0, -yL_[b] / 20.0) * makeup_[b];
            const double k = 1.0 - mix + mix * g;
            outL += bl[b] * k;
            outR += br[b] * k;
        }
        left[i] = static_cast<float>(outL);
        right[i] = static_cast<float>(outR);
    }
    grDb_ = static_cast<float>(maxGr);
}

}  // namespace rfx
