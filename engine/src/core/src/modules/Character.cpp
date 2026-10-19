// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Character.h"

#include <algorithm>
#include <cmath>

namespace rfx {

namespace {
int nextPow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
}  // namespace

// ---------------------------------------------------------------------------
// TransientShaper
// ---------------------------------------------------------------------------
void TransientShaper::prepare(double fs) {
    fs_ = fs;
    set(s_);
    reset();
}

void TransientShaper::reset() {
    e1_ = e2_ = e3_ = e4_ = 0.0;
    gainDb_ = 0.0;
}

void TransientShaper::set(const Settings& s) {
    s_ = s;
    const double k = std::pow(2.0, 1.0 - 2.0 * clampv(static_cast<double>(s_.speed), 0.0, 1.0));
    aFast_ = onePoleCoef(0.0005, fs_);
    aSlow_ = onePoleCoef(0.015 * k, fs_);
    rel_ = onePoleCoef(0.080 * k, fs_);
    rFast_ = onePoleCoef(0.030 * k, fs_);
    rSlow_ = onePoleCoef(0.300 * k, fs_);
    gainC_ = onePoleCoef(0.0005, fs_);
}

void TransientShaper::process(float* left, float* right, int n) {
    auto follow = [](double& e, double x, double a, double r) { e = x > e ? a * e + (1.0 - a) * x : r * e + (1.0 - r) * x; };
    const double atk = clampv(static_cast<double>(s_.attackDb), -15.0, 15.0);
    const double sus = clampv(static_cast<double>(s_.sustainDb), -15.0, 15.0);
    for (int i = 0; i < n; ++i) {
        const double x = std::max(std::fabs(left[i]), std::fabs(right[i]));
        // Attack pair: same release, different attack -> positive during onsets only.
        follow(e1_, x, aFast_, rel_);
        follow(e2_, x, aSlow_, rel_);
        // Sustain pair: same attack, different release -> positive during decays only.
        follow(e3_, x, aFast_, rFast_);
        follow(e4_, x, aFast_, rSlow_);
        const double dA = 20.0 * std::log10((e1_ + 1e-6) / (e2_ + 1e-6));
        const double dS = 20.0 * std::log10((e4_ + 1e-6) / (e3_ + 1e-6));
        // 1 dB dead zone absorbs detector ripple on steady tones; full effect at 7 dB.
        const double target = atk * clampv((dA - 1.0) / 6.0, 0.0, 1.0) + sus * clampv((dS - 1.0) / 6.0, 0.0, 1.0);
        gainDb_ = gainC_ * gainDb_ + (1.0 - gainC_) * target;
        const float g = static_cast<float>(std::pow(10.0, gainDb_ / 20.0));
        left[i] *= g;
        right[i] *= g;
    }
}

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

// ---------------------------------------------------------------------------
// TapeVinyl
// ---------------------------------------------------------------------------
void TapeVinyl::prepare(double fs) {
    fs_ = fs;
    const int size = nextPow2(static_cast<int>(0.03 * fs) + 8);
    dl_.assign(size, 0.0f);
    dr_.assign(size, 0.0f);
    mask_ = size - 1;
    set(s_);
    reset();
}

void TapeVinyl::reset() {
    std::fill(dl_.begin(), dl_.end(), 0.0f);
    std::fill(dr_.begin(), dr_.end(), 0.0f);
    wpos_ = 0;
    phWow_ = phFlutter_ = drift_ = 0.0;
    for (BiquadState* st : {&bumpL_, &bumpR_, &lossL1_, &lossL2_, &lossR1_, &lossR2_, &hissL_, &hissR_, &crackS_,
                            &rumbleS_, &dcL_, &dcR_})
        st->reset();
    crackEnv_ = 0.0f;
}

void TapeVinyl::set(const Settings& s) {
    s_ = s;
    const bool tape = s_.mode == 0;
    driveK_ = 1.0f + 4.0f * s_.drive * s_.drive;
    driveNorm_ = 1.0f / driveK_;
    bump_ = tape ? designRbj(FilterType::Peak, 75.0, 0.5 + 2.0 * s_.drive, 1.0, fs_) : BiquadCoeffs{};
    const double fc = tape ? 20000.0 - 14000.0 * s_.age : 18000.0 - 12000.0 * s_.age;
    loss_ = designRbj(FilterType::LowPass, std::min(fc, 0.45 * fs_), 0.0, 0.6, fs_);
    hissF_ = designRbj(FilterType::HighShelf, 4000.0, 6.0, 0.7071, fs_);
    crackF_ = designRbj(FilterType::BandPass, 2500.0, 0.0, 0.8, fs_);
    rumbleF_ = designRbj(FilterType::LowPass, 25.0, 0.0, 0.7071, fs_);
    dcF_ = designRbj(FilterType::HighPass1, 12.0, 0.0, 0.7071, fs_);
    hissGain_ = dbToGainf(tape ? -84.0f + 30.0f * s_.noise : -92.0f + 30.0f * s_.noise) * (s_.noise > 0 ? 1.0f : 0.0f);
    crackRate_ = tape ? 0.0f : (1.0f + 60.0f * s_.noise * (0.3f + s_.age)) / static_cast<float>(fs_);
    crackGain_ = s_.noise > 0 ? dbToGainf(-36.0f + 16.0f * s_.noise) : 0.0f;
    rumbleGain_ = tape ? 0.0f : dbToGainf(-66.0f + 12.0f * s_.noise) * s_.age;
}

int TapeVinyl::latency() const {
    const bool tape = s_.mode == 0;
    const double wowDepth = (tape ? 0.6e-3 : 0.9e-3) * s_.wow * fs_;
    const double flutDepth = (tape ? 0.05e-3 : 0.02e-3) * s_.flutter * fs_;
    return static_cast<int>(std::lround(0.006 * fs_ + wowDepth + flutDepth + 4.0));
}

void TapeVinyl::process(float* left, float* right, int n) {
    const bool tape = s_.mode == 0;
    const double wowRate = tape ? 0.5 : 0.5555;
    const double wowDepth = (tape ? 0.6e-3 : 0.9e-3) * s_.wow * fs_;
    const double flutDepth = (tape ? 0.05e-3 : 0.02e-3) * s_.flutter * fs_;
    const double base = 0.006 * fs_ + wowDepth + flutDepth + 4.0;
    const double incWow = wowRate / fs_, incFl = 6.3 / fs_;
    const double driftC = onePoleCoef(0.5, fs_);
    const float bias = 0.05f * s_.drive;
    const float biasOut = std::tanh(driveK_ * bias);
    const float norm = 1.0f / (driveK_ * (1.0f - biasOut * biasOut));
    for (int i = 0; i < n; ++i) {
        // Saturation (unity small-signal gain), head bump, gap/stylus loss.
        float l = (std::tanh(driveK_ * (left[i] + bias)) - biasOut) * norm;
        float r = (std::tanh(driveK_ * (right[i] + bias)) - biasOut) * norm;
        l = static_cast<float>(biquadTick(dcF_, dcL_, l));
        r = static_cast<float>(biquadTick(dcF_, dcR_, r));
        if (tape) {
            l = static_cast<float>(biquadTick(bump_, bumpL_, l));
            r = static_cast<float>(biquadTick(bump_, bumpR_, r));
        }
        l = static_cast<float>(biquadTick(loss_, lossL2_, biquadTick(loss_, lossL1_, l)));
        r = static_cast<float>(biquadTick(loss_, lossR2_, biquadTick(loss_, lossR1_, r)));

        // Wow and flutter: modulated delay, cubic Hermite read.
        dl_[wpos_] = l;
        dr_[wpos_] = r;
        drift_ = driftC * drift_ + (1.0 - driftC) * rnd();
        phWow_ += incWow;
        if (phWow_ >= 1.0) phWow_ -= 1.0;
        phFlutter_ += incFl * (1.0 + 0.1 * drift_);
        if (phFlutter_ >= 1.0) phFlutter_ -= 1.0;
        const double d = base + wowDepth * std::sin(kTwoPi * phWow_) + flutDepth * std::sin(kTwoPi * phFlutter_) +
                         0.3 * flutDepth * drift_;
        const double pos = wpos_ - d;
        const double fl = std::floor(pos);
        const int i1 = static_cast<int>(fl);
        const float t = static_cast<float>(pos - fl);
        auto hermite = [&](const std::vector<float>& b) {
            const float y0 = b[(i1 - 1) & mask_], y1 = b[i1 & mask_], y2 = b[(i1 + 1) & mask_], y3 = b[(i1 + 2) & mask_];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        };
        l = hermite(dl_);
        r = hermite(dr_);
        wpos_ = (wpos_ + 1) & mask_;

        // Noise: hiss / surface noise, crackle and rumble.
        if (hissGain_ > 0.0f) {
            l += hissGain_ * static_cast<float>(biquadTick(hissF_, hissL_, rnd()));
            r += hissGain_ * static_cast<float>(biquadTick(hissF_, hissR_, rnd()));
        }
        if (!tape && crackGain_ > 0.0f) {
            if (0.5f * (rnd() + 1.0f) < crackRate_) crackEnv_ = crackGain_ * (0.2f + 0.8f * 0.5f * (rnd() + 1.0f));
            const float c = static_cast<float>(biquadTick(crackF_, crackS_, crackEnv_ * rnd()));
            crackEnv_ *= 0.985f;
            const float pan = 0.5f + 0.4f * rnd();
            l += c * pan;
            r += c * (1.0f - pan);
            if (rumbleGain_ > 0.0f) {
                const float rb = rumbleGain_ * static_cast<float>(biquadTick(rumbleF_, rumbleS_, rnd()));
                l += rb;
                r += rb;
            }
        }
        left[i] = l;
        right[i] = r;
    }
}

// ---------------------------------------------------------------------------
// Declicker
// ---------------------------------------------------------------------------
void Declicker::prepare(double fs) {
    scaleCoef_ = onePoleCoef(0.020, fs);
    reset();
}

void Declicker::reset() {
    for (auto& c : ch_) {
        std::fill(std::begin(c.buf), std::end(c.buf), 0.0f);
        c.scale = 1e-4;
        c.clickStart = -1;
        c.clickLen = 0;
        c.quiet = 0;
    }
    t_ = 0;
    repairs_ = 0;
}

void Declicker::repair(Channel& c, int64_t start, int len) {
    const int64_t end = start + len - 1;
    const float p0 = c.buf[(start - 1) & (kSize - 1)];
    const float pm = c.buf[(start - 2) & (kSize - 1)];
    const float p1 = c.buf[(end + 1) & (kSize - 1)];
    const float pp = c.buf[(end + 2) & (kSize - 1)];
    const float span = static_cast<float>(len + 1);
    const float m0 = (p0 - pm) * span, m1 = (pp - p1) * span;
    for (int i = 1; i <= len; ++i) {
        const float t = i / span, t2 = t * t, t3 = t2 * t;
        const float h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
        c.buf[(start + i - 1) & (kSize - 1)] = h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
    }
    ++repairs_;
}

void Declicker::run(Channel& c, float x, int64_t t) {
    c.buf[t & (kSize - 1)] = x;
    const float d = x - 2.0f * c.buf[(t - 1) & (kSize - 1)] + c.buf[(t - 2) & (kSize - 1)];
    const double ad = std::fabs(d);
    const double thr = std::max(1e-3, c.scale * (6.0 + 24.0 * (1.0 - clampv(s_.sensitivity, 0.0f, 1.0f))));
    if (ad > thr) {
        if (c.clickStart < 0) c.clickStart = t;
        c.clickLen = static_cast<int>(t - c.clickStart) + 1;
        c.quiet = 0;
        if (c.clickLen > kMaxClick + 2) {  // too long for a click (a real transient): leave it
            c.clickStart = -1;
            c.clickLen = 0;
        }
        return;
    }
    if (c.clickStart >= 0) {
        if (++c.quiet >= 3) {
            repair(c, c.clickStart, std::max(1, c.clickLen - 2));
            c.clickStart = -1;
            c.clickLen = 0;
            c.quiet = 0;
        }
        return;
    }
    c.scale = scaleCoef_ * c.scale + (1.0 - scaleCoef_) * ad;
}

void Declicker::process(float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        run(ch_[0], left[i], t_);
        run(ch_[1], right[i], t_);
        left[i] = ch_[0].buf[(t_ - kDelay) & (kSize - 1)];
        right[i] = ch_[1].buf[(t_ - kDelay) & (kSize - 1)];
        ++t_;
    }
}

// ---------------------------------------------------------------------------
// ChannelDelay
// ---------------------------------------------------------------------------
void ChannelDelay::prepare(double fs) {
    fs_ = fs;
    const int size = nextPow2(static_cast<int>(0.051 * fs) + 2);
    bl_.assign(size, 0.0f);
    br_.assign(size, 0.0f);
    mask_ = size - 1;
    fadeLen_ = std::max(1, static_cast<int>(0.010 * fs));
    reset();
}

void ChannelDelay::reset() {
    std::fill(bl_.begin(), bl_.end(), 0.0f);
    std::fill(br_.begin(), br_.end(), 0.0f);
    pos_ = 0;
    fade_ = 0;
    oldL_ = dL_;
    oldR_ = dR_;
}

void ChannelDelay::set(float leftMs, float rightMs) {
    const int l = clampv(static_cast<int>(std::lround(leftMs * 1e-3 * fs_)), 0, mask_ - 1);
    const int r = clampv(static_cast<int>(std::lround(rightMs * 1e-3 * fs_)), 0, mask_ - 1);
    if (l == dL_ && r == dR_) return;
    oldL_ = dL_;
    oldR_ = dR_;
    dL_ = l;
    dR_ = r;
    fade_ = fadeLen_;
}

void ChannelDelay::process(float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        bl_[pos_] = left[i];
        br_[pos_] = right[i];
        float l = bl_[(pos_ - dL_) & mask_];
        float r = br_[(pos_ - dR_) & mask_];
        if (fade_ > 0) {
            const float t = static_cast<float>(fade_) / fadeLen_;
            l = l + (bl_[(pos_ - oldL_) & mask_] - l) * t;
            r = r + (br_[(pos_ - oldR_) & mask_] - r) * t;
            --fade_;
        }
        left[i] = l;
        right[i] = r;
        pos_ = (pos_ + 1) & mask_;
    }
}

}  // namespace rfx
