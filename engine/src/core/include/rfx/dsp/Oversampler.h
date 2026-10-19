// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// 2x polyphase half-band FIR oversampler for nonlinear stages (anti-aliasing).
// 63-tap Kaiser-windowed half-band; up + down latency = 31 base-rate samples.

#include <cmath>

#include "rfx/dsp/FirDesign.h"

namespace rfx {

class Halfband2x {
public:
    static constexpr int kTaps = 63;
    static constexpr int kCenter = 31;
    static constexpr int kEven = 32;          // non-zero taps h[0], h[2], ..., h[62]
    static constexpr int kLatency = 31;       // base-rate samples for an up/down pair

    Halfband2x() {
        double sum = 0.0;
        for (int j = 0; j < kEven; ++j) {
            const int i = 2 * j;
            const double x = (i - kCenter) * 0.5;
            const double s = std::sin(3.14159265358979323846 * x) / (3.14159265358979323846 * x);
            h_[j] = 0.5 * s * kaiserWindow((i - kCenter) / static_cast<double>(kCenter + 1), 8.0);
            sum += h_[j];
        }
        for (double& v : h_) v *= 0.5 / sum;  // exact unity DC gain
        reset();
    }

    void reset() {
        for (int i = 0; i < 2 * kEven; ++i) upHist_[i] = evenHist_[i] = 0.0;
        for (int i = 0; i < 2 * kOddDelay; ++i) oddHist_[i] = 0.0;
        upPos_ = evenPos_ = oddPos_ = 0;
    }

    /// One base-rate sample in, two oversampled samples out (y0 first in time).
    inline void up(float x, float& y0, float& y1) {
        upPos_ = (upPos_ == 0) ? kEven - 1 : upPos_ - 1;
        upHist_[upPos_] = x;
        upHist_[upPos_ + kEven] = x;
        const double* hp = upHist_ + upPos_;  // hp[k] = x[m-k]
        double acc = 0.0;
        for (int j = 0; j < kEven; ++j) acc += h_[j] * hp[j];
        y0 = static_cast<float>(2.0 * acc);
        y1 = static_cast<float>(hp[15]);  // centre tap: x[m-15]
    }

    /// Two oversampled samples in (x0 first in time), one base-rate sample out.
    inline float down(float x0, float x1) {
        evenPos_ = (evenPos_ == 0) ? kEven - 1 : evenPos_ - 1;
        evenHist_[evenPos_] = x0;
        evenHist_[evenPos_ + kEven] = x0;
        const double* ep = evenHist_ + evenPos_;  // ep[j] = E[m-j]
        double acc = 0.0;
        for (int j = 0; j < kEven; ++j) acc += h_[j] * ep[j];
        // Odd branch: centre tap 0.5 * O[m-16].
        oddPos_ = (oddPos_ == 0) ? kOddDelay - 1 : oddPos_ - 1;
        oddHist_[oddPos_] = x1;
        oddHist_[oddPos_ + kOddDelay] = x1;
        acc += 0.5 * oddHist_[oddPos_ + 16];
        return static_cast<float>(acc);
    }

private:
    static constexpr int kOddDelay = 17;
    double h_[kEven];
    double upHist_[2 * kEven];
    double evenHist_[2 * kEven];
    double oddHist_[2 * kOddDelay];
    int upPos_ = 0, evenPos_ = 0, oddPos_ = 0;
};

/// Integer delay line for latency-aligning dry paths.
template <int MaxDelay>
class FixedDelay {
public:
    void setDelay(int d) { delay_ = d < 0 ? 0 : (d > MaxDelay ? MaxDelay : d); }
    void reset() {
        for (float& v : buf_) v = 0.0f;
        pos_ = 0;
    }
    inline float process(float x) {
        buf_[pos_] = x;
        int r = pos_ - delay_;
        if (r < 0) r += MaxDelay + 1;
        const float y = buf_[r];
        if (++pos_ > MaxDelay) pos_ = 0;
        return y;
    }

private:
    float buf_[MaxDelay + 1] = {};
    int pos_ = 0, delay_ = 0;
};

}  // namespace rfx
