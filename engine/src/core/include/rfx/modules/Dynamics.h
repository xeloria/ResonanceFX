// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Dynamics: feed-forward compressor, BS.1770 loudness normaliser (AGC), true-peak
// look-ahead limiter and TPDF dither.

#include <cmath>
#include <cstdint>
#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"
#include "rfx/modules/Loudness.h"

namespace rfx {

/// Stereo-linked feed-forward compressor with soft knee and log-domain smooth-decoupled
/// peak detector (Giannoulis, Massberg & Reiss, JAES 60(6), 2012).  Side-chain high-pass
/// avoids bass-driven pumping; `mix` enables parallel ("New York") compression.
class Compressor {
public:
    struct Settings {
        float thresholdDb = -18.0f, ratio = 2.5f, attackMs = 10.0f, releaseMs = 150.0f, kneeDb = 6.0f;
        float makeupDb = 0.0f, mix = 1.0f, scHpfHz = 60.0f;
        bool autoMakeup = true;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    float gainReductionDb() const { return grDb_; }
    /// Gain already applied before the engine (dB, <= 0): the system volume on outputs where
    /// Android scales the mix before effects. Levels are judged on the programme itself, so the
    /// volume knob doesn't move the threshold.
    void setLevelOffsetDb(float db) { offsetDb_ = db; }

private:
    double gainComputer(double xDb) const;
    double offsetDb_ = 0.0;
    Settings s_;
    double fs_ = 48000.0;
    double aA_ = 0.0, aR_ = 0.0, y1_ = 0.0, yL_ = 0.0, makeup_ = 1.0;
    BiquadCoeffs hp_;
    BiquadState hpL_, hpR_;
    float grDb_ = 0.0f;
};

/// Loudness normaliser: rides a gain so that the K-weighted momentary loudness approaches the
/// target (EBU R128 / ITU-R BS.1770-4 measurement).  Gated below -50 LUFS so silence and fades
/// are never pumped up.  The first reading after a reset or a pause (300 ms of audio) sets the
/// gain directly, so a new track is levelled at once; after that it follows smoothly.  By default
/// it only lifts quiet programme (maxCutDb = 0); loud tracks are turned down only if allowed.
class Agc {
public:
    struct Settings {
        float targetLufs = -14.0f, maxGainDb = 9.0f, maxCutDb = 0.0f, speed = 0.5f;
    };
    void prepare(double sampleRate);
    /// Forgets everything: the next reading sets the gain from scratch.
    void reset();
    void set(const Settings& s) { s_ = s; }
    /// Gain already applied before the engine (dB, <= 0): the system volume on outputs where
    /// Android scales the mix before effects. Levels are judged on the programme itself, so the
    /// volume knob doesn't move the threshold.
    void setLevelOffsetDb(float db) { offsetDb_ = db; }
    void process(float* left, float* right, int n);
    float gainDb() const { return static_cast<float>(gainDb_); }
    double momentaryLufs() const { return meter_.momentaryLufs(); }

private:
    Settings s_;
    double fs_ = 48000.0;
    LoudnessMeter meter_;
    double gainDb_ = 0.0, offsetDb_ = 0.0;
    bool primed_ = false;  // gain is based on a real reading
    int silentBlocks_ = 0;
    LinearRamp ramp_;
};

/// Brick-wall look-ahead limiter.  Peak detection on 4x-interpolated samples estimates
/// inter-sample (true) peaks (ITU-R BS.1770-4 Annex 2).  Gain = moving average of a
/// sliding-window minimum, which provably never exceeds the required gain at any peak.
class Limiter {
public:
    struct Settings {
        float ceilingDb = -1.0f, releaseMs = 80.0f;
        bool truePeak = true;
        bool enabled = true;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    int latency() const { return lookahead_ + tpDelay_; }
    float gainReductionDb() const { return grDb_; }

private:
    inline double truePeak(float l, float r);
    Settings s_;
    double fs_ = 48000.0;
    int lookahead_ = 64;
    static constexpr int kTpTaps = 16;  // per phase
    int tpDelay_ = kTpTaps / 2;
    double tpCoef_[4][kTpTaps] = {};
    std::vector<float> tpHistL_, tpHistR_;
    int tpPos_ = 0;
    std::vector<float> delayL_, delayR_;
    int delayPos_ = 0, delayLen_ = 0;
    // sliding minimum (monotonic deque over a ring)
    std::vector<double> minVal_;
    std::vector<int64_t> minIdx_;
    int dqHead_ = 0, dqTail_ = 0, dqCap_ = 0;
    int64_t sampleIndex_ = 0;
    // moving average
    std::vector<double> avgBuf_;
    int avgPos_ = 0;
    double avgSum_ = 0.0;
    int avgResync_ = 0;
    double env_ = 1.0, relCoef_ = 0.0, ceiling_ = 0.89;
    float grDb_ = 0.0f;
};

/// TPDF dither + quantisation to 16 bit, optional first-order error-feedback noise shaping
/// (pushes the requantisation noise towards high frequencies where hearing is less sensitive).
class Dither {
public:
    void reset() { errL_ = errR_ = 0.0f; }
    inline int16_t quantize(float x, int channel, bool dither, bool shaping) {
        float& e = channel ? errR_ : errL_;
        float v = x * 32768.0f;
        if (shaping) v -= e;
        const float d = dither ? (rnd() - rnd()) : 0.0f;
        float q = std::floor(v + d + 0.5f);
        q = q < -32768.0f ? -32768.0f : (q > 32767.0f ? 32767.0f : q);
        if (shaping) e = clampv(q - v, -4.0f, 4.0f);
        return static_cast<int16_t>(q);
    }

private:
    inline float rnd() {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float>(state_ >> 8) * (1.0f / 16777216.0f);
    }
    uint32_t state_ = 0x12345678u;
    float errL_ = 0.0f, errR_ = 0.0f;
};

}  // namespace rfx
