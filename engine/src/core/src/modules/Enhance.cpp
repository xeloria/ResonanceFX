// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Enhance.h"

#include <cmath>

namespace rfx {

// ---------------------------------------------------------------------------
// BassEnhancer
// ---------------------------------------------------------------------------
void BassEnhancer::prepare(double fs) {
    fs_ = fs;
    design();
    reset();
}

void BassEnhancer::reset() {
    shL_.reset();
    shR_.reset();
    detS_.reset();
    detEnv_ = 0.0;
    bpHpS_.reset();
    bpLp1S_.reset();
    bpLp2S_.reset();
    hHpS_.reset();
    hHp2S_.reset();
    hLpS_.reset();
    env_ = 0.0;
    rL1_.reset();
    rL2_.reset();
    rR1_.reset();
    rR2_.reset();
    reductionDb_ = 0.0f;
    appliedGain_ = (s_.mode != kPsychoacoustic) ? s_.gainDb : 0.0f;
    shelf_ = designRbj(FilterType::LowShelf, s_.freq, appliedGain_, 0.7071, fs_);
    counter_ = 0;
}

void BassEnhancer::set(const Settings& s) {
    const bool redesign = s.freq != s_.freq || s.mode != s_.mode;
    s_ = s;
    s_.freq = clampv(s_.freq, 20.0f, 400.0f);
    s_.gainDb = clampv(s_.gainDb, 0.0f, 24.0f);
    s_.harmonics = clampv(s_.harmonics, 0.0f, 1.0f);
    s_.protect = clampv(s_.protect, 0.0f, 1.0f);
    if (redesign) design();
}

void BassEnhancer::design() {
    const double f = s_.freq;
    shelf_ = designRbj(FilterType::LowShelf, f, appliedGain_, 0.7071, fs_);
    detLp_ = designRbj(FilterType::LowPass, f, 0.0, 0.7071, fs_);
    detAtt_ = onePoleCoef(0.010, fs_);
    detRel_ = onePoleCoef(0.250, fs_);
    bpHp_ = designRbj(FilterType::HighPass, std::max(20.0, f * 0.3), 0.0, 0.7071, fs_);
    bpLp1_ = designRbj(FilterType::LowPass, f, 0.0, 0.7071, fs_);
    bpLp2_ = bpLp1_;
    envRel_ = onePoleCoef(0.030, fs_);
    hHp_ = designRbj(FilterType::HighPass, f, 0.0, 0.7071, fs_);
    hLp_ = designRbj(FilterType::LowPass, std::min(f * 6.0, 0.45 * fs_), 0.0, 0.7071, fs_);
    rHp_ = designRbj(FilterType::HighPass, f, 0.0, 0.7071, fs_);
}

double BassEnhancer::staticResponseDb(const Settings& s, double freq, double fs) {
    if (s.mode == kPsychoacoustic) return 0.0;
    return designRbj(FilterType::LowShelf, s.freq, s.gainDb, 0.7071, fs).magnitudeDb(freq, fs);
}

void BassEnhancer::process(float* left, float* right, int n) {
    const bool useShelf = s_.mode != kPsychoacoustic;
    const bool useHarm = s_.mode != kShelf && s_.harmonics > 0.0f;
    const double hGain = 1.5 * s_.harmonics;
    for (int i = 0; i < n; ++i) {
        double l = left[i], r = right[i];
        const double m = 0.5 * (l + r);

        if (useShelf) {
            const double d = std::fabs(biquadTick(detLp_, detS_, m));
            const double c = d > detEnv_ ? detAtt_ : detRel_;
            detEnv_ = c * detEnv_ + (1.0 - c) * d;
            if (++counter_ >= 32) {
                counter_ = 0;
                // Boosted bass level estimate; back the boost off above -3 dBFS.
                const double level = gainToDb(detEnv_ * 1.41, -120.0);
                const double over = level + s_.gainDb + 3.0;
                const double red = s_.protect * std::max(0.0, over);
                const double want = std::max(0.0, s_.gainDb - red);
                const double cur = appliedGain_;
                double next = cur + clampv(want - cur, -0.5, 0.25);  // fast back-off, slow recovery
                if (std::fabs(next - cur) > 0.02) {
                    appliedGain_ = static_cast<float>(next);
                    shelf_ = designRbj(FilterType::LowShelf, s_.freq, appliedGain_, 0.7071, fs_);
                }
                reductionDb_ = static_cast<float>(s_.gainDb - appliedGain_);
            }
        }

        double h = 0.0;
        if (useHarm) {
            const double b = biquadTick(bpLp2_, bpLp2S_, biquadTick(bpLp1_, bpLp1S_, biquadTick(bpHp_, bpHpS_, m)));
            const double a = std::fabs(b);
            env_ = a > env_ ? a : envRel_ * env_ + (1.0 - envRel_) * a;
            const double x = env_ > 1e-7 ? clampv(b / env_, -1.0, 1.0) : 0.0;
            const double x2 = x * x;
            const double t2 = 2.0 * x2 - 1.0;
            const double t3 = x * (4.0 * x2 - 3.0);
            const double t4 = 8.0 * x2 * (x2 - 1.0) + 1.0;
            h = (t2 + 0.6 * t3 + 0.3 * t4) * env_;
            h = biquadTick(hHp_, hHp2S_, biquadTick(hHp_, hHpS_, h));
            h = biquadTick(hLp_, hLpS_, h) * hGain;
        }

        if (s_.replace) {
            l = biquadTick(rHp_, rL2_, biquadTick(rHp_, rL1_, l));
            r = biquadTick(rHp_, rR2_, biquadTick(rHp_, rR1_, r));
        }
        if (useShelf && appliedGain_ > 0.0f) {
            l = biquadTick(shelf_, shL_, l);
            r = biquadTick(shelf_, shR_, r);
        }
        left[i] = static_cast<float>(l + h);
        right[i] = static_cast<float>(r + h);
    }
}

// ---------------------------------------------------------------------------
// Clarity
// ---------------------------------------------------------------------------
void Clarity::prepare(double fs) {
    fs_ = fs;
    design();
    dryL_.setDelay(Halfband2x::kLatency);
    dryR_.setDelay(Halfband2x::kLatency);
    reset();
}

void Clarity::reset() {
    srcL_.reset();
    srcR_.reset();
    outL_.reset();
    outR_.reset();
    out2L_.reset();
    out2R_.reset();
    airL_.reset();
    airR_.reset();
    upL_.reset();
    upR_.reset();
    dnL_.reset();
    dnR_.reset();
    dryL_.reset();
    dryR_.reset();
    envL_ = envR_ = 0.0;
}

void Clarity::set(const Settings& s) {
    const bool redesign = s.mode != s_.mode || s.freq != s_.freq || s.airDb != s_.airDb;
    s_ = s;
    s_.freq = clampv(s_.freq, 500.0f, static_cast<float>(0.45 * fs_));
    s_.amount = clampv(s_.amount, 0.0f, 1.0f);
    s_.airDb = clampv(s_.airDb, 0.0f, 12.0f);
    if (redesign) design();
}

void Clarity::design() {
    const double src = s_.mode == 0 ? s_.freq : s_.freq * 0.5;
    srcHp_ = designRbj(FilterType::HighPass, src, 0.0, 0.7071, fs_);
    outHp_ = designRbj(FilterType::HighPass, std::min<double>(s_.freq, 0.45 * fs_), 0.0, 0.7071, fs_);
    air_ = designMatched(FilterType::HighShelf, std::min(12000.0, 0.4 * fs_), s_.airDb, 0.7071, fs_);
    envAtt_ = onePoleCoef(0.0005, 2.0 * fs_);
    envRel_ = onePoleCoef(0.015, 2.0 * fs_);
}

double Clarity::staticResponseDb(const Settings& s, double freq, double fs) {
    if (s.airDb <= 0.0f) return 0.0;
    return designMatched(FilterType::HighShelf, std::min(12000.0, 0.4 * fs), s.airDb, 0.7071, fs)
        .magnitudeDb(freq, fs);
}

void Clarity::process(float* left, float* right, int n) {
    const double amt = s_.amount;
    auto gen = [this](double a, double& env) {
        const double m = std::fabs(a);
        const double c = m > env ? envAtt_ : envRel_;
        env = c * env + (1.0 - c) * m;
        const double x = env > 1e-7 ? clampv(a / env, -1.0, 1.0) : 0.0;
        const double x2 = x * x;
        return (0.7 * (2.0 * x2 - 1.0) + 0.5 * x * (4.0 * x2 - 3.0)) * env;
    };
    for (int i = 0; i < n; ++i) {
        const float sl = static_cast<float>(biquadTick(srcHp_, srcL_, left[i]));
        const float sr = static_cast<float>(biquadTick(srcHp_, srcR_, right[i]));
        float a0, a1, b0, b1;
        upL_.up(sl, a0, a1);
        upR_.up(sr, b0, b1);
        const float hl = dnL_.down(static_cast<float>(gen(a0, envL_)), static_cast<float>(gen(a1, envL_)));
        const float hr = dnR_.down(static_cast<float>(gen(b0, envR_)), static_cast<float>(gen(b1, envR_)));
        const double el = biquadTick(outHp_, out2L_, biquadTick(outHp_, outL_, hl));
        const double er = biquadTick(outHp_, out2R_, biquadTick(outHp_, outR_, hr));
        double l = dryL_.process(left[i]) + amt * el;
        double r = dryR_.process(right[i]) + amt * er;
        if (s_.airDb > 0.0f) {
            l = biquadTick(air_, airL_, l);
            r = biquadTick(air_, airR_, r);
        }
        left[i] = static_cast<float>(l);
        right[i] = static_cast<float>(r);
    }
}

// ---------------------------------------------------------------------------
// Tube
// ---------------------------------------------------------------------------
void Tube::prepare(double fs) {
    fs_ = fs;
    dc_ = designRbj(FilterType::HighPass1, 10.0, 0.0, 0.7071, fs_);
    dryL_.setDelay(Halfband2x::kLatency);
    dryR_.setDelay(Halfband2x::kLatency);
    set(s_);
    reset();
}

void Tube::reset() {
    upL_.reset();
    upR_.reset();
    dnL_.reset();
    dnR_.reset();
    dryL_.reset();
    dryR_.reset();
    dcL_.reset();
    dcR_.reset();
}

void Tube::set(const Settings& s) {
    s_ = s;
    s_.drive = clampv(s_.drive, 0.0f, 1.0f);
    s_.even = clampv(s_.even, 0.0f, 1.0f);
    s_.mix = clampv(s_.mix, 0.0f, 1.0f);
    k_ = 1.0f + 4.0f * s_.drive * s_.drive;
    bias_ = 0.3f * s_.even;
    const float tb = std::tanh(k_ * bias_);
    offset_ = tb;
    norm_ = 1.0f / (k_ * (1.0f - tb * tb));
}

inline float Tube::shape(float x) const { return (std::tanh(k_ * (x + bias_)) - offset_) * norm_; }

void Tube::process(float* left, float* right, int n) {
    const float wet = s_.mix, dry = 1.0f - s_.mix;
    for (int i = 0; i < n; ++i) {
        float a0, a1, b0, b1;
        upL_.up(left[i], a0, a1);
        upR_.up(right[i], b0, b1);
        const float l = dnL_.down(shape(a0), shape(a1));
        const float r = dnR_.down(shape(b0), shape(b1));
        const float wl = static_cast<float>(biquadTick(dc_, dcL_, l));
        const float wr = static_cast<float>(biquadTick(dc_, dcR_, r));
        const float dl = dryL_.process(left[i]);
        const float dr = dryR_.process(right[i]);
        left[i] = wet * wl + dry * dl;
        right[i] = wet * wr + dry * dr;
    }
}

}  // namespace rfx
