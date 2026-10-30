// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

// Blind A/B switching for the in-app listening test.

#include <algorithm>
#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"

namespace rfx::fx {

/// Switches between two renderings of the same input, A (the reference: effects off, or the
/// effect under test off) and B (the full chain), so that nothing but the effect gives the
/// answer away:
///  - Time: the earlier rendering is delayed by the latency difference, so switching never jumps.
///  - Level: both are measured continuously with the ITU-R BS.1770 K-weighting (moving average
///    over the last 3 s of programme, silence gated) and the louder one is turned down to match -
///    louder otherwise sounds "better" whatever the effect does.
///  - Switching: a 30 ms crossfade, so a click doesn't reveal that the rendering changed.
class AbxSwitch {
public:
    static constexpr int kMaxDelay = 8192;  // samples; more than any chain latency at 192 kHz

    void prepare(double sampleRate);
    void reset();
    void select(bool chain) { chain_ = chain; }
    /// A and B are the two renderings of one block; the result is written over B.
    void process(const float* aL, const float* aR, int latencyA, float* bL, float* bR, int latencyB, int n);
    /// Loudness of B minus A in dB, once at least `kSettleSeconds` of programme was measured.
    bool measured() const { return filled_ >= kChunks; }
    double deltaDb() const;

private:
    static constexpr int kChunks = 30;  // 100 ms each: the 3 s window
    struct Delay {
        std::vector<float> buf = std::vector<float>(kMaxDelay, 0.0f);
        int pos = 0;
        float tick(float x, int d) {
            buf[static_cast<size_t>(pos)] = x;
            const float y = buf[static_cast<size_t>((pos - d + kMaxDelay) & (kMaxDelay - 1))];
            pos = (pos + 1) & (kMaxDelay - 1);
            return y;
        }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); pos = 0; }
    };
    double kWeightedPower(const float* l, const float* r, int n, BiquadState* st);

    double fs_ = 48000.0;
    bool chain_ = true;
    Delay dAL_, dAR_, dBL_, dBR_;
    BiquadCoeffs shelf_, hp_;
    BiquadState kA_[4], kB_[4];
    // Gated K-weighted energy per 100 ms chunk, for both renderings.
    double chunkA_[kChunks] = {}, chunkB_[kChunks] = {};
    double accA_ = 0.0, accB_ = 0.0;
    int accFrames_ = 0, chunkFrames_ = 4800, chunk_ = 0, filled_ = 0;
    LinearRamp gainA_, gainB_, mix_;
};

}  // namespace rfx::fx
