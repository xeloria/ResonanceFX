// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Dynamics.h"

#include <cmath>

#include "rfx/dsp/FirDesign.h"

namespace rfx {

// ---------------------------------------------------------------------------
// Compressor
// ---------------------------------------------------------------------------
void Compressor::prepare(double fs) {
    fs_ = fs;
    set(s_);
    reset();
}

void Compressor::reset() {
    y1_ = yL_ = 0.0;
    hpL_.reset();
    hpR_.reset();
    grDb_ = 0.0f;
}

void Compressor::set(const Settings& s) {
    s_ = s;
    s_.ratio = clampv(s_.ratio, 1.0f, 50.0f);
    s_.kneeDb = clampv(s_.kneeDb, 0.0f, 30.0f);
    aA_ = onePoleCoef(clampv(s_.attackMs, 0.05f, 500.0f) * 1e-3, fs_);
    aR_ = onePoleCoef(clampv(s_.releaseMs, 5.0f, 5000.0f) * 1e-3, fs_);
    hp_ = designRbj(FilterType::HighPass, clampv(s_.scHpfHz, 10.0f, 1000.0f), 0.0, 0.7071, fs_);
    double m = s_.makeupDb;
    if (s_.autoMakeup) m += -0.5 * gainComputer(0.0);  // half of the gain lost at 0 dBFS
    makeup_ = dbToGain(m);
}

double Compressor::gainComputer(double x) const {
    const double T = s_.thresholdDb, R = s_.ratio, W = s_.kneeDb;
    const double d = x - T;
    if (2.0 * d < -W) return x;
    if (W > 0.0 && 2.0 * std::fabs(d) <= W) {
        const double t = d + W / 2.0;
        return x + (1.0 / R - 1.0) * t * t / (2.0 * W);
    }
    return T + d / R;
}

void Compressor::process(float* left, float* right, int n) {
    const double mix = s_.mix;
    double maxGr = 0.0;
    for (int i = 0; i < n; ++i) {
        const double sl = biquadTick(hp_, hpL_, left[i]);
        const double sr = biquadTick(hp_, hpR_, right[i]);
        const double peak = std::max(std::fabs(sl), std::fabs(sr));
        const double xDb = 20.0 * std::log10(std::max(peak, 1e-6));
        const double xL = xDb - gainComputer(xDb);
        y1_ = std::max(xL, aR_ * y1_ + (1.0 - aR_) * xL);
        yL_ = aA_ * yL_ + (1.0 - aA_) * y1_;
        maxGr = std::max(maxGr, yL_);
        const double g = std::pow(10.0, -yL_ / 20.0) * makeup_;
        const double k = 1.0 - mix + mix * g;
        left[i] = static_cast<float>(left[i] * k);
        right[i] = static_cast<float>(right[i] * k);
    }
    grDb_ = static_cast<float>(maxGr);
}

// ---------------------------------------------------------------------------
// Agc
// ---------------------------------------------------------------------------
void Agc::prepare(double fs) {
    fs_ = fs;
    meter_.prepare(fs);
    reset();
}

void Agc::reset() {
    meter_.reset();
    gainDb_ = 0.0;
    ramp_.snap(1.0f);
}

void Agc::process(float* left, float* right, int n) {
    meter_.process(left, right, n);
    if (meter_.blockReady()) {
        const double m = meter_.momentaryLufs();
        if (m > -50.0) {
            const double desired = clampv(static_cast<double>(s_.targetLufs) - m, -12.0,
                                          static_cast<double>(s_.maxGainDb));
            const double sp = clampv(static_cast<double>(s_.speed), 0.0, 1.0);
            const double tauDown = 4.0 + (0.5 - 4.0) * sp;   // reduce gain: 4 s .. 0.5 s
            const double tauUp = 12.0 + (2.0 - 12.0) * sp;   // increase gain: 12 s .. 2 s
            const double tau = desired < gainDb_ ? tauDown : tauUp;
            gainDb_ += (1.0 - std::exp(-0.1 / tau)) * (desired - gainDb_);
        }
        ramp_.setTarget(dbToGainf(static_cast<float>(gainDb_)), static_cast<int>(0.1 * fs_));
    }
    for (int i = 0; i < n; ++i) {
        const float g = ramp_.next();
        left[i] *= g;
        right[i] *= g;
    }
}

// ---------------------------------------------------------------------------
// Limiter
// ---------------------------------------------------------------------------
void Limiter::prepare(double fs) {
    fs_ = fs;
    lookahead_ = std::max(8, static_cast<int>(std::lround(0.0015 * fs)));
    tpDelay_ = kTpTaps / 2;
    // 4-phase windowed-sinc interpolator: phase p estimates x(n - D + p/4).
    for (int p = 0; p < 4; ++p) {
        double sum = 0.0;
        for (int j = 0; j < kTpTaps; ++j) {
            const double x = j - tpDelay_ + p * 0.25;
            const double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(kPi * x) / (kPi * x);
            const double w = kaiserWindow(x / (kTpTaps / 2.0 + 0.5), 6.0);
            tpCoef_[p][j] = s * w;
            sum += tpCoef_[p][j];
        }
        for (int j = 0; j < kTpTaps; ++j) tpCoef_[p][j] /= sum;
    }
    tpHistL_.assign(2 * kTpTaps, 0.0f);
    tpHistR_.assign(2 * kTpTaps, 0.0f);
    delayLen_ = lookahead_;
    delayL_.assign(delayLen_, 0.0f);
    delayR_.assign(delayLen_, 0.0f);
    const int W = lookahead_ + 1;
    dqCap_ = W + 2;
    minVal_.assign(dqCap_, 1.0);
    minIdx_.assign(dqCap_, 0);
    avgBuf_.assign(W, 1.0);
    set(s_);
    reset();
}

void Limiter::reset() {
    std::fill(tpHistL_.begin(), tpHistL_.end(), 0.0f);
    std::fill(tpHistR_.begin(), tpHistR_.end(), 0.0f);
    tpPos_ = 0;
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    delayPos_ = 0;
    dqHead_ = dqTail_ = 0;
    sampleIndex_ = 0;
    std::fill(avgBuf_.begin(), avgBuf_.end(), 1.0);
    avgPos_ = 0;
    avgSum_ = static_cast<double>(avgBuf_.size());
    avgResync_ = 0;
    env_ = 1.0;
    grDb_ = 0.0f;
}

void Limiter::set(const Settings& s) {
    s_ = s;
    ceiling_ = dbToGain(clampv(s_.ceilingDb, -24.0f, 0.0f));
    relCoef_ = onePoleCoef(clampv(s_.releaseMs, 1.0f, 5000.0f) * 1e-3, fs_);
}

inline double Limiter::truePeak(float l, float r) {
    tpPos_ = (tpPos_ == 0) ? kTpTaps - 1 : tpPos_ - 1;
    tpHistL_[tpPos_] = l;
    tpHistL_[tpPos_ + kTpTaps] = l;
    tpHistR_[tpPos_] = r;
    tpHistR_[tpPos_ + kTpTaps] = r;
    const float* hl = tpHistL_.data() + tpPos_;  // hl[j] = x[n-j]
    const float* hr = tpHistR_.data() + tpPos_;
    double pk = std::max(std::fabs(hl[tpDelay_]), std::fabs(hr[tpDelay_]));
    if (s_.truePeak) {
        for (int p = 1; p < 4; ++p) {
            double al = 0.0, ar = 0.0;
            for (int j = 0; j < kTpTaps; ++j) {
                al += tpCoef_[p][j] * hl[j];
                ar += tpCoef_[p][j] * hr[j];
            }
            pk = std::max(pk, std::max(std::fabs(al), std::fabs(ar)));
        }
    }
    return pk;
}

void Limiter::process(float* left, float* right, int n) {
    const int W = lookahead_ + 1;
    double minGain = 1.0;
    for (int i = 0; i < n; ++i) {
        const double pk = truePeak(left[i], right[i]);
        // Audio aligned with `pk` (delayed by tpDelay_).
        const float al = tpHistL_[tpPos_ + tpDelay_];
        const float ar = tpHistR_[tpPos_ + tpDelay_];

        double g = 1.0;
        if (s_.enabled) {
            const double greq = pk > ceiling_ ? ceiling_ / pk : 1.0;
            // Sliding-window minimum over the last W required gains.
            while (dqTail_ != dqHead_) {
                const int last = (dqTail_ - 1 + dqCap_) % dqCap_;
                if (minVal_[last] >= greq) dqTail_ = last; else break;
            }
            minVal_[dqTail_] = greq;
            minIdx_[dqTail_] = sampleIndex_;
            dqTail_ = (dqTail_ + 1) % dqCap_;
            while (minIdx_[dqHead_] <= sampleIndex_ - W) dqHead_ = (dqHead_ + 1) % dqCap_;
            const double gmin = minVal_[dqHead_];
            env_ = gmin < env_ ? gmin : gmin + (env_ - gmin) * relCoef_;
            avgSum_ += env_ - avgBuf_[avgPos_];
            avgBuf_[avgPos_] = env_;
            if (++avgPos_ == W) avgPos_ = 0;
            if (++avgResync_ >= 8192) {
                avgResync_ = 0;
                double s = 0.0;
                for (double v : avgBuf_) s += v;
                avgSum_ = s;
            }
            g = std::min(1.0, avgSum_ / W);
            minGain = std::min(minGain, g);
        }
        ++sampleIndex_;

        const float dl = delayL_[delayPos_];
        const float dr = delayR_[delayPos_];
        delayL_[delayPos_] = al;
        delayR_[delayPos_] = ar;
        if (++delayPos_ == delayLen_) delayPos_ = 0;
        float ol = static_cast<float>(dl * g);
        float orr = static_cast<float>(dr * g);
        if (s_.enabled) {
            const float c = static_cast<float>(ceiling_);
            ol = clampv(ol, -c, c);
            orr = clampv(orr, -c, c);
        }
        left[i] = ol;
        right[i] = orr;
    }
    grDb_ = static_cast<float>(-gainToDb(minGain, -60.0));
}

}  // namespace rfx
