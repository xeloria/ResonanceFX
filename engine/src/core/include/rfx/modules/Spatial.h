// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Spatial processing: stereo image (M/S width + mono bass), headphone crossfeed and an
// 8-line feedback-delay-network reverb.

#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"
#include "rfx/modules/Multiband.h"

namespace rfx {

class StereoImage {
public:
    struct Settings {
        float width = 1.0f;
        float bassMonoHz = 0.0f;
        float haasMs = 0.0f;       // > 0 delays the right channel, < 0 the left (precedence effect)
        float decorrelate = 0.0f;  // Lauridsen complementary-comb pseudo-stereo amount
        float widthLow = 1.0f, widthMid = 1.0f, widthHigh = 1.0f;
        float lowXover = 250.0f, highXover = 4000.0f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);

private:
    Settings s_;
    double fs_ = 48000.0;
    LinearRamp width_, decor_;
    BiquadCoeffs hp_, decorHp_;
    BiquadState s1_, s2_, dh1_, dh2_;
    LrCrossover xo_;
    bool bandWidths_ = false;
    std::vector<float> decorBuf_, haasBuf_;
    int decorMask_ = 0, decorDelay_ = 0, haasMask_ = 0, haasDelay_ = 0, pos_ = 0;
};

/// Headphone crossfeed.
///  - Bauer / BS2B: first-order low-passed crossfeed plus a complementary high-boost on the
///    direct path; at low frequencies the crossfeed sits `feedDb` below the direct signal
///    (Bauer, JAES 1961; Mikhaylov's bs2b parameterisation).
///  - Spherical head: Brown & Duda (1998) head-shadow model and Woodworth ITD for a pair
///    of loudspeakers at +/- `angle`, applied as contralateral-to-ipsilateral transfer.
class Crossfeed {
public:
    struct Settings {
        int mode = 0;
        float cutoffHz = 700.0f, feedDb = 4.5f;
        float angleDeg = 30.0f, amount = 0.6f, headRadiusCm = 8.75f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    /// Interaural delay (seconds) used by the spherical-head model, for diagnostics/tests.
    static double itdSeconds(double angleDeg, double headRadiusM);

private:
    void design();
    Settings s_;
    double fs_ = 48000.0;
    // bs2b
    double a0lo_ = 0, b1lo_ = 0, a0hi_ = 0, a1hi_ = 0, b1hi_ = 0, gain_ = 1;
    double loL_ = 0, loR_ = 0, hiL_ = 0, hiR_ = 0, asisL_ = 0, asisR_ = 0;
    // spherical head
    BiquadCoeffs shadow_;
    BiquadState shL_, shR_;
    std::vector<float> dL_, dR_;
    int dPos_ = 0;
    double delay_ = 0.0, amount_ = 0.0, norm_ = 1.0;
};

/// Feedback delay network reverb (Jot & Chaigne 1991): 8 modulated delay lines, Householder
/// feedback matrix, per-line absorption filters giving frequency-dependent RT60, Schroeder
/// all-pass input diffusion, and a sparse early-reflection pattern.
class Reverb {
public:
    struct Settings {
        float size = 0.4f, decayS = 1.2f, damping = 0.5f, predelayMs = 12.0f, early = 0.5f;
        float wetDb = -18.0f, dryDb = 0.0f, width = 1.0f, lowcutHz = 150.0f, highcutHz = 9000.0f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);

private:
    static constexpr int kLines = 8;
    static constexpr int kDiffusers = 4;
    static constexpr int kTaps = 12;
    struct Line {
        std::vector<float> buf;
        int mask = 0;
        double length = 1000.0, target = 1000.0;
        double g = 0.5, b = 0.0, lp = 0.0;
    };
    struct Allpass {
        std::vector<float> buf;
        int len = 1, pos = 0;
        float g = 0.6f;
    };
    void updateDecay();
    Settings s_;
    double fs_ = 48000.0;
    Line lines_[kLines];
    Allpass diff_[kDiffusers];
    std::vector<float> pre_;
    int preMask_ = 0, writePos_ = 0;
    double preDelay_ = 0.0;
    int tapDelay_[kTaps] = {};
    float tapGainL_[kTaps] = {}, tapGainR_[kTaps] = {};
    BiquadCoeffs inHp_, inLp_;
    BiquadState inHpS_, inLpS_;
    double lfoPhase_[2] = {0.0, 0.25}, lfoInc_[2] = {0.0, 0.0}, modDepth_ = 0.0;
    LinearRamp wet_, dry_;
};

}  // namespace rfx
