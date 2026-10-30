// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#include "Abx.h"

#include <algorithm>
#include <cmath>

#include "rfx/modules/Loudness.h"

namespace rfx::fx {

void AbxSwitch::prepare(double sampleRate) {
    fs_ = sampleRate;
    chunkFrames_ = std::max(1, static_cast<int>(0.1 * fs_));
    shelf_ = LoudnessMeter::kWeightingShelf(fs_);
    hp_ = LoudnessMeter::kWeightingHighPass(fs_);
    reset();
}

void AbxSwitch::reset() {
    for (Delay* d : {&dAL_, &dAR_, &dBL_, &dBR_}) d->clear();
    for (int i = 0; i < 4; ++i) {
        kA_[i].reset();
        kB_[i].reset();
    }
    std::fill(std::begin(chunkA_), std::end(chunkA_), 0.0);
    std::fill(std::begin(chunkB_), std::end(chunkB_), 0.0);
    accA_ = accB_ = 0.0;
    accFrames_ = chunk_ = filled_ = 0;
    gainA_.snap(1.0f);
    gainB_.snap(1.0f);
    mix_.snap(chain_ ? 1.0f : 0.0f);
}

double AbxSwitch::deltaDb() const {
    double a = 0.0, b = 0.0;
    for (int i = 0; i < std::min(filled_, kChunks); ++i) {
        a += chunkA_[i];
        b += chunkB_[i];
    }
    return a > 0.0 && b > 0.0 ? 10.0 * std::log10(b / a) : 0.0;
}

double AbxSwitch::kWeightedPower(const float* l, const float* r, int n, BiquadState* st) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double kl = biquadTick(hp_, st[1], biquadTick(shelf_, st[0], l[i]));
        const double kr = biquadTick(hp_, st[3], biquadTick(shelf_, st[2], r[i]));
        sum += kl * kl + kr * kr;
    }
    return n > 0 ? sum / n : 0.0;
}

void AbxSwitch::process(const float* aL, const float* aR, int latencyA, float* bL, float* bR, int latencyB, int n) {
    if (n <= 0) return;
    // Measure before the alignment delays: the averages don't care about a few ms of offset.
    const double pa = kWeightedPower(aL, aR, n, kA_);
    const double pb = kWeightedPower(bL, bR, n, kB_);
    constexpr double kGate = 1e-7;  // about -70 LUFS: silence and fades don't count
    if (pa > kGate && pb > kGate) {
        accA_ += pa * n;
        accB_ += pb * n;
        accFrames_ += n;
        if (accFrames_ >= chunkFrames_) {
            chunkA_[chunk_] = accA_;
            chunkB_[chunk_] = accB_;
            chunk_ = (chunk_ + 1) % kChunks;
            if (filled_ < kChunks) ++filled_;
            accA_ = accB_ = 0.0;
            accFrames_ = 0;
        }
    }
    // Turn the louder rendering down; never boost (that could clip).
    const double delta = deltaDb();
    gainA_.setTarget(static_cast<float>(dbToGain(std::min(0.0, delta))), n);
    gainB_.setTarget(static_cast<float>(dbToGain(std::min(0.0, -delta))), n);
    mix_.setTarget(chain_ ? 1.0f : 0.0f, static_cast<int>(0.03 * fs_));

    const int lat = std::max(latencyA, latencyB);
    const int dA = std::clamp(lat - latencyA, 0, kMaxDelay - 1);
    const int dB = std::clamp(lat - latencyB, 0, kMaxDelay - 1);
    for (int i = 0; i < n; ++i) {
        const float ga = gainA_.next(), gb = gainB_.next(), m = mix_.next();
        const float al = dAL_.tick(aL[i], dA) * ga, ar = dAR_.tick(aR[i], dA) * ga;
        const float bl = dBL_.tick(bL[i], dB) * gb, br = dBR_.tick(bR[i], dB) * gb;
        bL[i] = al + (bl - al) * m;
        bR[i] = ar + (br - ar) * m;
    }
}

}  // namespace rfx::fx
