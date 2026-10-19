// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/DynamicEq.h"

#include <algorithm>
#include <cmath>

namespace rfx {

namespace {
constexpr int kUpdate = 16;  // samples between filter-gain updates

FilterType mainType(int t) {
    return t == 1 ? FilterType::LowShelf : (t == 2 ? FilterType::HighShelf : FilterType::Peak);
}

// Soft-knee "amount over threshold" (6 dB quadratic knee).
inline double softOver(double over) {
    const double W = 6.0;
    if (over <= -W / 2) return 0.0;
    if (over >= W / 2) return over;
    const double t = over + W / 2;
    return t * t / (2 * W);
}
}  // namespace

int decodeDynamicBands(const float* v, int count, DynamicBand* out, int maxBands) {
    const int n = std::min(count / 10, maxBands);
    for (int i = 0; i < n; ++i) {
        const float* p = v + i * 10;
        DynamicBand b;
        b.enabled = p[0] >= 0.5f;
        b.type = clampv(static_cast<int>(std::lround(p[1])), 0, 2);
        b.freq = std::isfinite(p[2]) ? p[2] : 1000.0f;
        b.q = clampv(std::isfinite(p[3]) ? p[3] : 1.0f, 0.1f, 20.0f);
        b.thresholdDb = clampv(p[4], -80.0f, 0.0f);
        b.rangeDb = clampv(p[5], -24.0f, 24.0f);
        b.ratio = clampv(p[6], 1.0f, 50.0f);
        b.attackMs = clampv(p[7], 0.1f, 500.0f);
        b.releaseMs = clampv(p[8], 5.0f, 5000.0f);
        b.mode = p[9] >= 0.5f ? 1 : 0;
        out[i] = b;
    }
    return n;
}

void DynamicEq::prepare(double fs) {
    fs_ = fs;
    for (int i = 0; i < count_; ++i) configure(slots_[i]);
    reset();
}

void DynamicEq::reset() {
    for (auto& s : slots_) {
        s.scS.reset();
        s.mL.reset();
        s.mR.reset();
        s.env = -120.0;
        s.applied = 0.0f;
        s.main = BiquadCoeffs{};
    }
    activity_ = 0.0f;
}

void DynamicEq::configure(Slot& s) {
    const double f = clampv<double>(s.b.freq, 20.0, 0.45 * fs_);
    switch (s.b.type) {
        case 1: s.sc = designRbj(FilterType::LowPass, f, 0.0, 0.7071, fs_); break;
        case 2: s.sc = designRbj(FilterType::HighPass, f, 0.0, 0.7071, fs_); break;
        default: s.sc = designRbj(FilterType::BandPass, f, 0.0, s.b.q, fs_); break;
    }
    s.aA = onePoleCoef(s.b.attackMs * 1e-3, fs_);
    s.aR = onePoleCoef(s.b.releaseMs * 1e-3, fs_);
    s.main = designRbj(mainType(s.b.type), f, s.applied, s.b.q, fs_);
}

void DynamicEq::setBands(const DynamicBand* bands, int n) {
    n = std::min(n, kMaxBands);
    for (int i = 0; i < n; ++i) {
        Slot& s = slots_[i];
        const bool typeChanged = s.b.type != bands[i].type;
        s.b = bands[i];
        if (typeChanged) {
            s.scS.reset();
            s.applied = 0.0f;
        }
        configure(s);
    }
    for (int i = n; i < kMaxBands; ++i) slots_[i].b.enabled = false;
    count_ = n;
}

void DynamicEq::process(float* left, float* right, int n) {
    float act = 0.0f;
    for (int off = 0; off < n; off += kUpdate) {
        const int m = std::min(kUpdate, n - off);
        for (int k = 0; k < count_; ++k) {
            Slot& s = slots_[k];
            if (!s.b.enabled) continue;
            // Side-chain level (peak envelope, dB).
            for (int i = 0; i < m; ++i) {
                const double x = 0.5 * (left[off + i] + right[off + i]);
                const double y = std::fabs(biquadTick(s.sc, s.scS, x));
                const double yDb = 20.0 * std::log10(std::max(y, 1e-7));
                const double c = yDb > s.env ? s.aA : s.aR;
                s.env = c * s.env + (1.0 - c) * yDb;
            }
            const double over = s.b.mode == 0 ? s.env - s.b.thresholdDb : s.b.thresholdDb - s.env;
            const double amount = softOver(over) * (1.0 - 1.0 / s.b.ratio);
            const double target = s.b.rangeDb < 0 ? -std::min(amount, -static_cast<double>(s.b.rangeDb))
                                                  : std::min(amount, static_cast<double>(s.b.rangeDb));
            if (std::fabs(target - s.applied) > 0.05) {
                s.applied = static_cast<float>(target);
                s.main = designRbj(mainType(s.b.type), clampv<double>(s.b.freq, 20.0, 0.45 * fs_), s.applied, s.b.q, fs_);
            }
            act = std::max(act, std::fabs(s.applied));
            if (s.applied != 0.0f || s.mL.s1 != 0.0 || s.mR.s1 != 0.0) {
                for (int i = 0; i < m; ++i) {
                    left[off + i] = static_cast<float>(biquadTick(s.main, s.mL, left[off + i]));
                    right[off + i] = static_cast<float>(biquadTick(s.main, s.mR, right[off + i]));
                }
            }
        }
    }
    activity_ = act;
}

// ---------------------------------------------------------------------------
void DeEsser::prepare(double fs) {
    fs_ = fs;
    set(s_);
    reset();
}

void DeEsser::reset() {
    sc1_.reset();
    sc2_.reset();
    shL_.reset();
    shR_.reset();
    env_ = -120.0;
    applied_ = reduction_ = 0.0f;
    shelf_ = BiquadCoeffs{};
}

void DeEsser::set(const Settings& s) {
    s_ = s;
    s_.freq = clampv(s_.freq, 2000.0f, static_cast<float>(0.42 * fs_));
    sc_ = designRbj(FilterType::HighPass, s_.freq, 0.0, 0.7071, fs_);
    aA_ = onePoleCoef(0.0005, fs_);
    aR_ = onePoleCoef(0.060, fs_);
}

void DeEsser::process(float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const double x = 0.5 * (left[i] + right[i]);
        const double y = std::fabs(biquadTick(sc_, sc2_, biquadTick(sc_, sc1_, x)));
        const double yDb = 20.0 * std::log10(std::max(y, 1e-7));
        const double c = yDb > env_ ? aA_ : aR_;
        env_ = c * env_ + (1.0 - c) * yDb;
        if (++counter_ >= kUpdate) {
            counter_ = 0;
            const double over = softOver(env_ - s_.thresholdDb);
            const float red = static_cast<float>(std::min(over * 0.75, static_cast<double>(s_.amountDb)));
            if (std::fabs(red - applied_) > 0.05f) {
                applied_ = red;
                if (s_.mode == 0)
                    shelf_ = designRbj(FilterType::HighShelf, s_.freq * 0.85, -applied_, 0.7071, fs_);
            }
            reduction_ = applied_;
        }
        if (s_.mode == 0) {
            if (applied_ > 0.0f || shL_.s1 != 0.0) {
                left[i] = static_cast<float>(biquadTick(shelf_, shL_, left[i]));
                right[i] = static_cast<float>(biquadTick(shelf_, shR_, right[i]));
            }
        } else if (applied_ > 0.0f) {
            const float g = dbToGainf(-applied_);
            left[i] *= g;
            right[i] *= g;
        }
    }
}

}  // namespace rfx
