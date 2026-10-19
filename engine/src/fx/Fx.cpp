// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#include "Fx.h"

#include <algorithm>
#include <cmath>

#include "rfx/dsp/Common.h"

namespace rfx::fx {

// ---- BiquadCascade ----------------------------------------------------------------------------

void BiquadCascade::prepare(double sampleRate) {
    fs_ = sampleRate;
    rebuild();
}

void BiquadCascade::reset() {
    for (auto& s : state_) s.fill(0.0);
}

bool BiquadCascade::setCoefficients(uint32_t count, const float* at44k1, const float* at48k) {
    if (count == 0 || count % 5 != 0 || count / 5 > kMaxSections) return false;
    auto load = [count](const float* c, std::vector<Section>& out) {
        out.clear();
        for (uint32_t i = 0; i < count; i += 5) {
            Section s{c[i], c[i + 1], c[i + 2], -static_cast<double>(c[i + 3]), -static_cast<double>(c[i + 4])};
            if (!std::isfinite(s.b0) || !std::isfinite(s.b1) || !std::isfinite(s.b2) || !std::isfinite(s.a1) ||
                !std::isfinite(s.a2)) {
                return false;
            }
            out.push_back(s);
        }
        return true;
    };
    std::vector<Section> a, b;
    if (!load(at44k1, a) || !load(at48k, b)) return false;
    src44_ = std::move(a);
    src48_ = std::move(b);
    rebuild();
    return true;
}

void BiquadCascade::rebuild() {
    if (std::fabs(fs_ - 44100.0) < 1.0) {
        active_ = src44_;
    } else if (std::fabs(fs_ - 48000.0) < 1.0 || src48_.empty()) {
        active_ = src48_;
    } else {
        // Re-target each 48 kHz section: z-domain -> analog prototype (inverse bilinear at 48 kHz)
        // -> z-domain at the output rate. Exact in shape below a few kHz; near Nyquist the two
        // warpings differ slightly, which matters little for headphone correction curves.
        active_.clear();
        const double k1 = 2.0 * 48000.0, k2 = 2.0 * fs_;
        for (const Section& s : src48_) {
            // Polynomials in z: N = b0 z^2 + b1 z + b2, D = z^2 + a1 z + a2.
            // z = (k1 + s)/(k1 - s): N(s) = n2 s^2 + n1 s + n0.
            auto toAnalog = [k1](double c0, double c1, double c2, double& p2, double& p1, double& p0) {
                p2 = c0 - c1 + c2;
                p1 = 2.0 * k1 * (c0 - c2);
                p0 = k1 * k1 * (c0 + c1 + c2);
            };
            double n2, n1, n0, d2, d1, d0;
            toAnalog(s.b0, s.b1, s.b2, n2, n1, n0);
            toAnalog(1.0, s.a1, s.a2, d2, d1, d0);
            // s = k2 (z - 1)/(z + 1): P(z) = (p2 k2^2 + p1 k2 + p0) z^2 + 2(p0 - p2 k2^2) z
            //                              + (p2 k2^2 - p1 k2 + p0)
            auto toDigital = [k2](double p2, double p1, double p0, double& c0, double& c1, double& c2) {
                c0 = p2 * k2 * k2 + p1 * k2 + p0;
                c1 = 2.0 * (p0 - p2 * k2 * k2);
                c2 = p2 * k2 * k2 - p1 * k2 + p0;
            };
            double b0, b1, b2, a0, a1, a2;
            toDigital(n2, n1, n0, b0, b1, b2);
            toDigital(d2, d1, d0, a0, a1, a2);
            if (std::fabs(a0) < 1e-30) continue;
            active_.push_back({b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0});
        }
    }
    state_.assign(active_.size(), {});
}

void BiquadCascade::process(float* left, float* right, int n) {
    const size_t k = active_.size();
    if (k == 0) return;
    for (int i = 0; i < n; ++i) {
        double l = left[i], r = right[i];
        for (size_t j = 0; j < k; ++j) {
            const Section& c = active_[j];
            auto& s = state_[j];
            const double yl = c.b0 * l + c.b1 * s[0] + c.b2 * s[1] - c.a1 * s[2] - c.a2 * s[3];
            s[1] = s[0]; s[0] = l; s[3] = s[2]; s[2] = yl; l = yl;
            const double yr = c.b0 * r + c.b1 * s[4] + c.b2 * s[5] - c.a1 * s[6] - c.a2 * s[7];
            s[5] = s[4]; s[4] = r; s[7] = s[6]; s[6] = yr; r = yr;
        }
        left[i] = static_cast<float>(l);
        right[i] = static_cast<float>(r);
    }
}

// ---- FieldSurround ----------------------------------------------------------------------------

void FieldSurround::prepare(double sampleRate) {
    fs_ = sampleRate;
    hp_ = designRbj(FilterType::HighPass, 180.0, 0.0, 0.7071, fs_);
    reset();
}

void FieldSurround::reset() {
    hpL_.reset();
    hpR_.reset();
}

void FieldSurround::set(float strength, float midLevel) {
    // Strength 0..1 widens the side signal by up to +8 dB (x2.5); mid 0..1 sets the centre image
    // from -6 dB to +3.5 dB with 0.5 = unity.
    sideGain_ = 1.0f + 1.5f * clampv(strength, 0.0f, 1.0f);
    const float m = clampv(midLevel, 0.0f, 1.0f);
    midGain_ = m <= 0.5f ? 0.5f + m : 1.0f + (m - 0.5f);
}

void FieldSurround::process(float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const double mid = 0.5 * (left[i] + right[i]);
        const double side = 0.5 * (left[i] - right[i]);
        // Only the side signal above ~180 Hz is boosted; bass side content stays as it was.
        const double sideHigh = biquadTick(hp_, hpL_, side);
        const double sideOut = side + (sideGain_ - 1.0) * sideHigh;
        const double midOut = mid * midGain_;
        left[i] = static_cast<float>(midOut + sideOut);
        right[i] = static_cast<float>(midOut - sideOut);
    }
}

// ---- HaasDelay ----------------------------------------------------------------------------------

void HaasDelay::prepare(double sampleRate) {
    fs_ = sampleRate;
    buf_.assign(static_cast<size_t>(std::ceil(0.021 * fs_)) + 1, 0.0f);
    pos_ = 0;
    setDelayMs(static_cast<float>(delay_ * 1000.0 / fs_));
}

void HaasDelay::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    pos_ = 0;
}

void HaasDelay::setDelayMs(float ms) {
    delay_ = static_cast<int>(std::lround(clampv(ms, 0.0f, 20.0f) * fs_ / 1000.0));
    delay_ = std::min(delay_, static_cast<int>(buf_.size()) - 1);
}

void HaasDelay::process(float* left, float* right, int n) {
    (void) left;
    if (delay_ <= 0 || buf_.empty()) return;
    const int size = static_cast<int>(buf_.size());
    for (int i = 0; i < n; ++i) {
        buf_[pos_] = right[i];
        int read = pos_ - delay_;
        if (read < 0) read += size;
        right[i] = buf_[read];
        if (++pos_ == size) pos_ = 0;
    }
}

// ---- DynamicBass ----------------------------------------------------------------------------------

void DynamicBass::prepare(double sampleRate) {
    fs_ = sampleRate;
    envCoef_ = onePoleCoef(0.3, fs_);     // programme envelopes
    smoothCoef_ = onePoleCoef(0.15, fs_ / 64.0);  // per-update smoothing (updates every 64 samples)
    set(s_);
    reset();
}

void DynamicBass::reset() {
    for (BiquadState* s : {&detLpS_, &detHpS_, &shelfL_, &shelfR_, &lowMidL_, &lowMidR_, &presL_, &presR_}) s->reset();
    bassEnv_ = fullEnv_ = 0.0;
    boostDb_ = 0.0;
    appliedBoostDb_ = -1.0;
    counter_ = 0;
}

void DynamicBass::set(const Settings& s) {
    s_ = s;
    s_.bassHz = clampv(s_.bassHz, 30.0f, 200.0f);
    s_.bassQ = clampv(s_.bassQ, 0.2f, 1.2f);
    s_.lowMidHz = clampv(s_.lowMidHz, 150.0f, 1200.0f);
    s_.presenceHz = clampv(s_.presenceHz, 2000.0f, 10000.0f);
    s_.strength = clampv(s_.strength, 0.0f, 1.0f);
    detLp_ = designRbj(FilterType::LowPass, s_.bassHz * 1.5, 0.0, 0.7071, fs_);
    detHp_ = designRbj(FilterType::HighPass, 25.0, 0.0, 0.7071, fs_);
    appliedBoostDb_ = -1.0;  // force coefficient refresh
}

void DynamicBass::updateShelf(double boostDb) {
    shelf_ = designRbj(FilterType::LowShelf, s_.bassHz, boostDb, s_.bassQ + 0.2, fs_);
    // Keep the overall balance: a little low-mid dip and presence lift that scale with the boost.
    lowMid_ = designRbj(FilterType::Peak, s_.lowMidHz, -0.25 * boostDb * clampv(s_.lowMidTrim, 0.0f, 1.2f), 0.8, fs_);
    presence_ = designRbj(FilterType::HighShelf, s_.presenceHz, 0.15 * boostDb * clampv(s_.presenceTrim, 0.0f, 1.2f),
                          0.7071, fs_);
    appliedBoostDb_ = boostDb;
}

void DynamicBass::process(float* left, float* right, int n) {
    const double maxBoost = 16.0 * s_.strength;
    for (int i = 0; i < n; ++i) {
        const double mono = 0.5 * (left[i] + right[i]);
        const double bass = biquadTick(detLp_, detLpS_, biquadTick(detHp_, detHpS_, mono));
        bassEnv_ = envCoef_ * bassEnv_ + (1.0 - envCoef_) * bass * bass;
        fullEnv_ = envCoef_ * fullEnv_ + (1.0 - envCoef_) * mono * mono;
        if (++counter_ >= 64) {
            counter_ = 0;
            // Bass share of the programme: ~0.5 for bass-heavy music, ~0.05 for thin material.
            // Half the boost is always there; the other half comes in as the bass thins out.
            const double share = fullEnv_ > 1e-12 ? bassEnv_ / fullEnv_ : 0.5;
            const double want = maxBoost * (0.5 + 0.5 * clampv(1.0 - share / 0.5, 0.0, 1.0));
            boostDb_ = smoothCoef_ * boostDb_ + (1.0 - smoothCoef_) * want;
            if (std::fabs(boostDb_ - appliedBoostDb_) > 0.1) updateShelf(boostDb_);
        }
        if (appliedBoostDb_ < 0.0) updateShelf(0.0);
        double l = biquadTick(shelf_, shelfL_, left[i]);
        double r = biquadTick(shelf_, shelfR_, right[i]);
        l = biquadTick(lowMid_, lowMidL_, l);
        r = biquadTick(lowMid_, lowMidR_, r);
        left[i] = static_cast<float>(biquadTick(presence_, presL_, l));
        right[i] = static_cast<float>(biquadTick(presence_, presR_, r));
    }
}

// ---- AnalogColor --------------------------------------------------------------------------------

void AnalogColor::prepare(double sampleRate) {
    fs_ = sampleRate;
    tone_.prepare(fs_);
    tone_.setMatched(true);
    setLevel(level_);
}

void AnalogColor::reset() { tone_.reset(); }

void AnalogColor::setLevel(int level) {
    level_ = clampv(level, 0, 2);
    static constexpr float kDrive[3] = {0.12f, 0.25f, 0.45f};
    static constexpr float kLowDb[3] = {1.0f, 1.8f, 2.5f};
    static constexpr float kHighDb[3] = {-0.5f, -1.0f, -1.6f};
    drive_ = kDrive[level_];
    EqBand bands[2];
    bands[0] = {true, FilterType::LowShelf, 110.0f, kLowDb[level_], 0.6f, 0};
    bands[1] = {true, FilterType::HighShelf, 9000.0f, kHighDb[level_], 0.7f, 0};
    tone_.setBands(bands, 2);
}

void AnalogColor::process(float* left, float* right, int n) {
    // Asymmetric soft saturation (adds mostly 2nd harmonic), level-compensated so that the
    // effect colours rather than louder-sounds-better.
    const float k = drive_ * 4.0f;
    const float norm = 1.0f / std::tanh(k);
    const float bias = 0.15f * drive_;
    const float biasOut = std::tanh(k * bias) * norm;
    auto shape = [&](float x) {
        const float y = std::tanh(k * (x + bias)) * norm - biasOut;
        return 0.6f * x + 0.4f * y;
    };
    for (int i = 0; i < n; ++i) {
        left[i] = shape(left[i]);
        right[i] = shape(right[i]);
    }
    tone_.process(left, right, n);
}

// ---- SpeakerVoicing -------------------------------------------------------------------------------

void SpeakerVoicing::prepare(double sampleRate) {
    eq_.prepare(sampleRate);
    eq_.setMatched(true);
    EqBand bands[3];
    // Phone speakers roll off below ~250 Hz; energy down there only adds distortion.
    bands[0] = {true, FilterType::HighPass, 150.0f, 0.0f, 0.7071f, 0};
    // Upper-bass / low-mid lift where they still respond, presence lift for intelligibility.
    bands[1] = {true, FilterType::Peak, 320.0f, 3.0f, 1.0f, 0};
    bands[2] = {true, FilterType::Peak, 3000.0f, 2.0f, 1.2f, 0};
    eq_.setBands(bands, 3);
}

void SpeakerVoicing::reset() { eq_.reset(); }

void SpeakerVoicing::process(float* left, float* right, int n) { eq_.process(left, right, n); }

}  // namespace rfx::fx
