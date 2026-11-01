// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Character.h"

#include <algorithm>
#include <cmath>

namespace rfx {

// ---------------------------------------------------------------------------
// SubHarmonic
// ---------------------------------------------------------------------------
void SubHarmonic::prepare(double fs) {
    fs_ = fs;
    set(s_);
    reset();
}

void SubHarmonic::reset() {
    for (BiquadState* st : {&hpS_, &lp1S_, &lp2S_, &o1S_, &o2S_, &o3S_, &dcS_}) st->reset();
    env_ = prev_ = 0.0;
    ff_ = 1.0;
    armed_ = true;
}

void SubHarmonic::set(const Settings& s) {
    s_ = s;
    const double f = clampv(static_cast<double>(s_.freq), 40.0, 200.0);
    hp_ = designRbj(FilterType::HighPass, std::max(25.0, f * 0.4), 0.0, 0.7071, fs_);
    lp_ = designRbj(FilterType::LowPass, f, 0.0, 0.7071, fs_);
    out_ = designRbj(FilterType::LowPass, f * 0.5, 0.0, 0.7071, fs_);
    dc_ = designRbj(FilterType::HighPass1, 15.0, 0.0, 0.7071, fs_);
    envRel_ = onePoleCoef(0.050, fs_);
}

void SubHarmonic::process(float* left, float* right, int n) {
    const double amt = 1.2 * s_.amount;
    for (int i = 0; i < n; ++i) {
        const double m = 0.5 * (left[i] + right[i]);
        const double xb = biquadTick(lp_, lp2S_, biquadTick(lp_, lp1S_, biquadTick(hp_, hpS_, m)));
        const double a = std::fabs(xb);
        env_ = a > env_ ? a : envRel_ * env_ + (1.0 - envRel_) * a;
        // Flip-flop on positive-going zero crossings, with hysteresis against noise.
        if (xb < -0.1 * env_) armed_ = true;
        if (armed_ && prev_ < 0.0 && xb >= 0.0) {
            ff_ = -ff_;
            armed_ = false;
        }
        prev_ = xb;
        double sub = env_ * ff_;
        sub = biquadTick(out_, o3S_, biquadTick(out_, o2S_, biquadTick(out_, o1S_, sub)));
        sub = biquadTick(dc_, dcS_, sub) * amt;
        left[i] = static_cast<float>(left[i] + sub);
        right[i] = static_cast<float>(right[i] + sub);
    }
}

// ---------------------------------------------------------------------------
// SoftClipper
// ---------------------------------------------------------------------------
void SoftClipper::prepare(double) {
    set(s_);
    reset();
}

void SoftClipper::reset() {
    upL_.reset();
    upR_.reset();
    dnL_.reset();
    dnR_.reset();
}

void SoftClipper::set(const Settings& s) {
    s_ = s;
    t_ = dbToGainf(clampv(s_.thresholdDb, -24.0f, 0.0f));
    k_ = clampv(s_.softness, 0.0f, 1.0f) * 0.5f;
}

inline float SoftClipper::clip(float x) const {
    const float u = x / t_;
    const float a = std::fabs(u);
    float y;
    if (k_ < 1e-4f) {
        y = std::min(a, 1.0f);
    } else if (a <= 1.0f - k_) {
        y = a;
    } else if (a < 1.0f + k_) {
        const float d = a - 1.0f + k_;
        y = a - d * d / (4.0f * k_);
    } else {
        y = 1.0f;
    }
    return (u < 0.0f ? -y : y) * t_;
}

void SoftClipper::process(float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        float a0, a1, b0, b1;
        upL_.up(left[i], a0, a1);
        upR_.up(right[i], b0, b1);
        left[i] = dnL_.down(clip(a0), clip(a1));
        right[i] = dnR_.down(clip(b0), clip(b1));
    }
}

}  // namespace rfx
