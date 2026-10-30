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

/// Speaker protection, in one of two modes.
///  - Model (default): an excursion limiter.  Below its resonance a closed-box driver's cone
///    excursion follows the drive voltage through a second-order low-pass at the system
///    resonance (Small, JAES 20(5), 1972), so that filter's output predicts excursion.  When the
///    prediction exceeds the limit, only the band below the crossover (1.4x resonance) is turned
///    down: bass gets cleaner and the driver stops rattling, while voices and detail keep their
///    level.  A subsonic high-pass at half the resonance removes content the driver can't
///    reproduce but would still have to move for.
///  - Measured: per-band ceilings from the user's own distortion measurement (the loudest level
///    each band played below 5 % THD).  The signal is split into kBands phase-coherent bands and
///    each band is limited at its own ceiling, so only what actually distorts is held back.
class SpeakerProtector {
public:
    static constexpr int kBands = 5;
    /// Band edges of the measured mode: below 125 Hz, 125-250, 250-500, 500-1000, above 1 kHz.
    static constexpr double kCrossovers[kBands - 1] = {125.0, 250.0, 500.0, 1000.0};

    struct Settings {
        float resonanceHz = 700.0f;  // driver + enclosure resonance
        float limitDb = -12.0f;      // excursion ceiling, dB re full scale
        bool measured = false;
        float ceilingDb[kBands] = {0, 0, 0, 0, 0};  // measured mode: peak level per band (>= 0: no limit)
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
    // Measured mode.
    void processMeasured(float* left, float* right, int n);
    LrCrossover bands_;
    double bandEnvDb_[kBands] = {}, bandGainDb_[kBands] = {};
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
        float orientation = 1.0f;  // 0..1: fades the effect when the speakers aren't left and right of the listener
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
