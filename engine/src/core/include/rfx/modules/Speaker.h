// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Loudspeaker processing: excursion protection for small drivers and crosstalk cancellation
// for closely spaced speaker pairs.

#include <vector>

#include "rfx/dsp/Biquad.h"
#include "rfx/dsp/Common.h"
#include "rfx/modules/Multiband.h"

namespace rfx {

/// Excursion limiter.  Below its resonance a closed-box driver's cone excursion follows the
/// drive voltage through a second-order low-pass at the system resonance (Small, JAES 20(5),
/// 1972), so that filter's output predicts excursion.  When the prediction exceeds the limit,
/// only the band below the crossover (1.4x resonance) is turned down: bass gets cleaner and the
/// driver stops rattling, while voices and detail keep their level.  A subsonic high-pass at
/// half the resonance removes content the driver can't reproduce but would still have to move
/// for.
class SpeakerProtector {
public:
    struct Settings {
        float resonanceHz = 700.0f;  // driver + enclosure resonance
        float limitDb = -12.0f;      // excursion ceiling, dB re full scale
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    float gainReductionDb() const { return static_cast<float>(-gainDb_); }

private:
    void design();
    Settings s_;
    double fs_ = 48000.0;
    BiquadCoeffs subsonic_, excursion_;
    BiquadState subL_, subR_, excL_, excR_;
    LrCrossover xo_;
    double envDb_ = -150.0, gainDb_ = 0.0;
    double envRelease_ = 0.0, gainAttack_ = 0.0, gainRelease_ = 0.0;
};

/// Crosstalk cancellation for a speaker pair a few centimetres apart (phone, tablet, laptop,
/// compact stereo speaker).  Each ear also hears the opposite speaker slightly later, which
/// collapses the image to the width of the device.  Recursive cancellation (Glasgal's RACE,
/// after Atal & Schroeder 1963) feeds each output, inverted, delayed by the interaural path
/// difference and band-limited, into the opposite channel, cancelling the crosstalk and then
/// the crosstalk of the cancellation.  In mid/side terms that is s' = s + g*BP(s')[n - d] on
/// the side signal; the centre (voices, bass) is left untouched so the tonal balance holds.
class CrosstalkCanceller {
public:
    struct Settings {
        float amount = 0.5f;       // 0..1
        float spacingCm = 15.0f;   // distance between the two speakers
        float distanceCm = 40.0f;  // listener distance from the speakers
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    /// Interaural path difference of the crosstalk for this geometry, in seconds.
    static double delaySeconds(double spacingCm, double distanceCm);

private:
    static constexpr int kBufferSize = 512;  // power of two, > the longest delay at 192 kHz
    Settings s_;
    double fs_ = 48000.0;
    BiquadCoeffs hp_, lp_;
    BiquadState hpS_, lpS_;
    std::vector<float> buf_ = std::vector<float>(kBufferSize, 0.0f);
    int pos_ = 0;
    double delay_ = 4.0;
    LinearRamp gain_;
};

}  // namespace rfx
