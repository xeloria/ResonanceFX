// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Spectral.h"

#include <algorithm>
#include <cmath>

namespace rfx {

namespace {
int fftSizeFor(double fs) {
    int n = 256;
    while (n < fs * 0.021) n <<= 1;  // ~21 ms frames: 1024 at 44.1/48 kHz
    return n;
}
}  // namespace

void StftProcessor::prepare(int fftSize) {
    n_ = fftSize;
    hop_ = n_ / 4;
    fft_.init(n_);
    win_.resize(n_);
    for (int k = 0; k < n_; ++k) win_[k] = static_cast<float>(std::sqrt(0.5 - 0.5 * std::cos(kTwoPi * k / n_)));
    for (auto* v : {&inL_, &inR_, &tL_, &tR_, &accL_, &accR_}) v->assign(n_, 0.0f);
    outFifoL_.assign(hop_, 0.0f);
    outFifoR_.assign(hop_, 0.0f);
    for (auto* v : {&reL_, &imL_, &reR_, &imR_}) v->assign(n_ / 2 + 1, 0.0f);
    pos_ = 0;
}

void StftProcessor::reset() {
    for (auto* v : {&inL_, &inR_, &accL_, &accR_, &outFifoL_, &outFifoR_}) std::fill(v->begin(), v->end(), 0.0f);
    pos_ = 0;
}

// ---------------------------------------------------------------------------
void CentreExtractor::prepare(double fs) {
    fs_ = fs;
    stft_.prepare(fftSizeFor(fs));
    mask_.assign(stft_.bins(), 0.0f);
    set(s_);
}

void CentreExtractor::reset() {
    stft_.reset();
    std::fill(mask_.begin(), mask_.end(), 0.0f);
}

void CentreExtractor::set(const Settings& s) {
    s_ = s;
    gc_ = s_.centreDb <= -39.5f ? 0.0f : dbToGainf(s_.centreDb);
    ga_ = dbToGainf(s_.ambienceDb);
    gamma_ = 1.0f + 7.0f * clampv(s_.focus, 0.0f, 1.0f);
    const int n = stft_.size();
    const float lo = std::min(s_.lowHz, s_.highHz), hi = std::max(s_.lowHz, s_.highHz);
    k0_ = clampv(static_cast<int>(std::ceil(lo * n / fs_)), 0, stft_.bins() - 1);
    k1_ = clampv(static_cast<int>(std::floor(hi * n / fs_)), 0, stft_.bins() - 1);
}

void CentreExtractor::process(float* left, float* right, int n) {
    stft_.process(left, right, n, [this](float* rl, float* il, float* rr, float* ir) {
        for (int k = k0_; k <= k1_; ++k) {
            const float pl = rl[k] * rl[k] + il[k] * il[k];
            const float pr = rr[k] * rr[k] + ir[k] * ir[k];
            const float cross = rl[k] * rr[k] + il[k] * ir[k];
            const float coh = 2.0f * std::max(0.0f, cross) / (pl + pr + 1e-20f);
            const float m = std::pow(std::min(coh, 1.0f), gamma_);
            mask_[k] = 0.6f * mask_[k] + 0.4f * m;
            const float cr = mask_[k] * 0.5f * (rl[k] + rr[k]);
            const float ci = mask_[k] * 0.5f * (il[k] + ir[k]);
            rl[k] = gc_ * cr + ga_ * (rl[k] - cr);
            il[k] = gc_ * ci + ga_ * (il[k] - ci);
            rr[k] = gc_ * cr + ga_ * (rr[k] - cr);
            ir[k] = gc_ * ci + ga_ * (ir[k] - ci);
        }
    });
}

// ---------------------------------------------------------------------------
void SpectralDenoiser::prepare(double fs) {
    fs_ = fs;
    stft_.prepare(fftSizeFor(fs));
    const int b = stft_.bins();
    smooth_.assign(b, 0.0f);
    noise_.assign(b, 0.0f);
    gainPrev_.assign(b, 1.0f);
    gammaPrev_.assign(b, 1.0f);
    gain_.assign(b, 1.0f);
    eff_.assign(b, 0.0f);
    // Tonal guard: the per-bin floor may not exceed the local minimum across ~+/-200 Hz, so a
    // sustained note (stationary in time but narrow in frequency) is never mistaken for noise.
    guard_ = std::max(3, static_cast<int>(std::lround(200.0 / (fs / stft_.size()))));
    const double framesPerSecond = fs / (stft_.size() / 4.0);
    rise_ = static_cast<float>(std::pow(10.0, 3.0 / 10.0 / framesPerSecond));  // noise floor may rise 3 dB/s
    set(s_);
    reset();
}

void SpectralDenoiser::reset() {
    stft_.reset();
    std::fill(smooth_.begin(), smooth_.end(), 0.0f);
    std::fill(noise_.begin(), noise_.end(), 0.0f);
    std::fill(gainPrev_.begin(), gainPrev_.end(), 1.0f);
    std::fill(gammaPrev_.begin(), gammaPrev_.end(), 1.0f);
    frames_ = 0;
}

void SpectralDenoiser::set(const Settings& s) {
    s_ = s;
    floor_ = dbToGainf(-clampv(s_.amountDb, 0.0f, 40.0f));
    beta_ = std::pow(10.0f, (clampv(s_.sensitivity, 0.0f, 1.0f) - 0.5f) * 1.0f);
}

void SpectralDenoiser::process(float* left, float* right, int n) {
    stft_.process(left, right, n, [this](float* rl, float* il, float* rr, float* ir) {
        const int bins = stft_.bins();
        const bool warmup = frames_ < 8;
        for (int k = 0; k < bins; ++k) {
            const float p = 0.5f * (rl[k] * rl[k] + il[k] * il[k] + rr[k] * rr[k] + ir[k] * ir[k]);
            if (warmup) {
                smooth_[k] = frames_ == 0 ? p : 0.7f * smooth_[k] + 0.3f * p;
                noise_[k] = smooth_[k];
            } else {
                smooth_[k] = 0.7f * smooth_[k] + 0.3f * p;
                noise_[k] = smooth_[k] < noise_[k] ? smooth_[k] : noise_[k] * rise_;
            }
        }
        for (int k = 0; k < bins; ++k) {
            float m = noise_[k];
            for (int j = std::max(0, k - guard_), e = std::min(bins - 1, k + guard_); j <= e; ++j)
                m = std::min(m, noise_[j]);
            eff_[k] = std::min(noise_[k], 1.4f * m);  // 1.4: bias of a local minimum over smoothed estimates
        }
        for (int k = 0; k < bins; ++k) {
            const float p = 0.5f * (rl[k] * rl[k] + il[k] * il[k] + rr[k] * rr[k] + ir[k] * ir[k]);
            const float nz = std::max(beta_ * eff_[k], 1e-20f);
            const float gamma = p / nz;
            const float xi = 0.98f * gainPrev_[k] * gainPrev_[k] * gammaPrev_[k] + 0.02f * std::max(gamma - 1.0f, 0.0f);
            float g = xi / (1.0f + xi);
            g = std::max(g, floor_);
            gainPrev_[k] = g;
            gammaPrev_[k] = std::min(gamma, 1e4f);
            gain_[k] = g;
        }
        ++frames_;
        for (int k = 0; k < bins; ++k) {
            const float g = 0.25f * gain_[std::max(0, k - 1)] + 0.5f * gain_[k] + 0.25f * gain_[std::min(bins - 1, k + 1)];
            rl[k] *= g;
            il[k] *= g;
            rr[k] *= g;
            ir[k] *= g;
        }
    });
}

}  // namespace rfx
