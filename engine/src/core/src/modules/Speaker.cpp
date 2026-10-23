// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Speaker.h"

#include <algorithm>
#include <cmath>

namespace rfx {

// ---- SpeakerProtector ---------------------------------------------------------------------------

void SpeakerProtector::prepare(double sampleRate) {
    fs_ = sampleRate;
    // Envelope: instant attack, 30 ms release. Gain: 1 ms attack, 120 ms release, so the
    // limiting follows a kick drum without pumping the rest of the bass line.
    envRelease_ = onePoleCoef(0.030, fs_);
    gainAttack_ = onePoleCoef(0.001, fs_);
    gainRelease_ = onePoleCoef(0.120, fs_);
    design();
    reset();
}

void SpeakerProtector::reset() {
    subL_.reset();
    subR_.reset();
    excL_.reset();
    excR_.reset();
    xo_.reset();
    envDb_ = -150.0;
    gainDb_ = 0.0;
}

void SpeakerProtector::set(const Settings& s) {
    const bool redesign = s.resonanceHz != s_.resonanceHz;
    s_ = s;
    if (redesign) design();
}

void SpeakerProtector::design() {
    const double f0 = clampv<double>(s_.resonanceHz, 30.0, 2000.0);
    subsonic_ = designRbj(FilterType::HighPass, 0.5 * f0, 0.0, 0.70710678, fs_);
    excursion_ = designRbj(FilterType::LowPass, f0, 0.0, 0.70710678, fs_);
    const double xo = 1.4 * f0;
    xo_.configure(&xo, 1, fs_);
}

void SpeakerProtector::process(float* left, float* right, int n) {
    constexpr double kKneeDb = 3.0;
    const double limit = s_.limitDb;
    for (int i = 0; i < n; ++i) {
        const double l = biquadTick(subsonic_, subL_, left[i]);
        const double r = biquadTick(subsonic_, subR_, right[i]);
        const double x = std::max(std::fabs(biquadTick(excursion_, excL_, l)), std::fabs(biquadTick(excursion_, excR_, r)));
        const double xDb = gainToDb(x);
        envDb_ = xDb > envDb_ ? xDb : xDb + envRelease_ * (envDb_ - xDb);

        // Infinite ratio above the limit, with a soft knee.
        const double over = envDb_ - limit;
        double target = 0.0;
        if (over >= 0.5 * kKneeDb) target = -over;
        else if (over > -0.5 * kKneeDb) target = -(over + 0.5 * kKneeDb) * (over + 0.5 * kKneeDb) / (2.0 * kKneeDb);
        gainDb_ = target + (target < gainDb_ ? gainAttack_ : gainRelease_) * (gainDb_ - target);

        double bl[2], br[2];
        xo_.split(l, r, bl, br);
        const double g = gainDb_ < -0.001 ? dbToGain(gainDb_) : 1.0;
        left[i] = static_cast<float>(bl[0] * g + bl[1]);
        right[i] = static_cast<float>(br[0] * g + br[1]);
    }
}

// ---- CrosstalkCanceller -------------------------------------------------------------------------

double CrosstalkCanceller::delaySeconds(double spacingCm, double distanceCm) {
    constexpr double kEarCm = 8.75;  // half the ear spacing (average adult head radius)
    constexpr double kSoundCmPerS = 34300.0;
    const double half = 0.5 * spacingCm;
    const double ipsi = std::hypot(distanceCm, half - kEarCm);
    const double contra = std::hypot(distanceCm, half + kEarCm);
    return (contra - ipsi) / kSoundCmPerS;
}

void CrosstalkCanceller::prepare(double sampleRate) {
    fs_ = sampleRate;
    // The cancellation only works where the crosstalk is a clean delayed copy: not in the bass
    // (it would need huge gains, the speakers being nearly in phase there) and not in the top
    // octaves (the head shadows them and a few millimetres of head movement change the phase).
    // A first-order high-pass fades it out below 200 Hz without the phase reversal a steeper one
    // would add (which would turn the bass part of the loop into a cut).
    hp_ = designRbj(FilterType::HighPass1, 200.0, 0.0, 0.70710678, fs_);
    lp_ = designRbj(FilterType::LowPass, std::min(6000.0, 0.4 * fs_), 0.0, 0.70710678, fs_);
    set(s_);
    gain_.snap(gain_.target);
    reset();
}

void CrosstalkCanceller::reset() {
    hpS_.reset();
    lpS_.reset();
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    pos_ = 0;
}

void CrosstalkCanceller::set(const Settings& s) {
    s_ = s;
    const double d = delaySeconds(clampv(s.spacingCm, 2.0f, 100.0f), clampv(s.distanceCm, 10.0f, 500.0f)) * fs_;
    delay_ = clampv(d, 1.0, static_cast<double>(kBufferSize - 2));
    // Loop gain below 1 keeps the recursion stable (|BP| <= 1); 0.65 caps the side lift at about
    // +7 dB around 500 Hz-1 kHz for a phone held at arm's length.
    gain_.setTarget(0.65f * clampv(s.amount, 0.0f, 1.0f), static_cast<int>(0.02 * fs_));
}

void CrosstalkCanceller::process(float* left, float* right, int n) {
    constexpr int kMask = kBufferSize - 1;
    for (int i = 0; i < n; ++i) {
        const double m = 0.5 * (static_cast<double>(left[i]) + right[i]);
        const double s = 0.5 * (static_cast<double>(left[i]) - right[i]);
        const double g = gain_.next();

        // y[n - d] with linear interpolation (d >= 1, so both taps are already written).
        const double at = pos_ - delay_ + kBufferSize;
        const int i0 = static_cast<int>(at);
        const double frac = at - i0;
        const double y0 = buf_[static_cast<size_t>(i0 & kMask)];
        const double y1 = buf_[static_cast<size_t>((i0 + 1) & kMask)];
        const double sOut = s + g * (y0 + frac * (y1 - y0));

        buf_[static_cast<size_t>(pos_)] = static_cast<float>(biquadTick(lp_, lpS_, biquadTick(hp_, hpS_, sOut)));
        pos_ = (pos_ + 1) & kMask;

        left[i] = static_cast<float>(m + sOut);
        right[i] = static_cast<float>(m - sOut);
    }
}

}  // namespace rfx
