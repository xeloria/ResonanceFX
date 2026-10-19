// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Short-time Fourier processing (weighted overlap-add) and the spectral effects built on it:
// centre/ambience extraction and noise reduction.

#include <cstring>
#include <vector>

#include "rfx/dsp/Common.h"
#include "rfx/dsp/Fft.h"

namespace rfx {

/// Stereo STFT with sqrt-Hann analysis and synthesis windows at 75 % overlap (perfect
/// reconstruction).  Latency is exactly one frame (N samples).
class StftProcessor {
public:
    void prepare(int fftSize);
    void reset();
    int size() const { return n_; }
    int bins() const { return n_ / 2 + 1; }
    int latency() const { return n_; }

    /// fn(reL, imL, reR, imR) modifies the spectra of each frame in place.
    template <class Fn>
    void process(float* left, float* right, int n, Fn&& fn) {
        for (int i = 0; i < n; ++i) {
            inL_[n_ - hop_ + pos_] = left[i];
            inR_[n_ - hop_ + pos_] = right[i];
            left[i] = outFifoL_[pos_];
            right[i] = outFifoR_[pos_];
            if (++pos_ == hop_) {
                pos_ = 0;
                frame(fn);
            }
        }
    }

private:
    template <class Fn>
    void frame(Fn&& fn) {
        for (int k = 0; k < n_; ++k) {
            tL_[k] = inL_[k] * win_[k];
            tR_[k] = inR_[k] * win_[k];
        }
        fft_.forward(tL_.data(), reL_.data(), imL_.data());
        fft_.forward(tR_.data(), reR_.data(), imR_.data());
        fn(reL_.data(), imL_.data(), reR_.data(), imR_.data());
        fft_.inverse(reL_.data(), imL_.data(), tL_.data());
        fft_.inverse(reR_.data(), imR_.data(), tR_.data());
        for (int k = 0; k < n_; ++k) {
            accL_[k] += tL_[k] * win_[k] * 0.5f;  // sum of Hann at 75 % overlap = 2
            accR_[k] += tR_[k] * win_[k] * 0.5f;
        }
        std::memcpy(outFifoL_.data(), accL_.data(), hop_ * sizeof(float));
        std::memcpy(outFifoR_.data(), accR_.data(), hop_ * sizeof(float));
        std::memmove(accL_.data(), accL_.data() + hop_, (n_ - hop_) * sizeof(float));
        std::memmove(accR_.data(), accR_.data() + hop_, (n_ - hop_) * sizeof(float));
        std::fill(accL_.begin() + (n_ - hop_), accL_.end(), 0.0f);
        std::fill(accR_.begin() + (n_ - hop_), accR_.end(), 0.0f);
        std::memmove(inL_.data(), inL_.data() + hop_, (n_ - hop_) * sizeof(float));
        std::memmove(inR_.data(), inR_.data() + hop_, (n_ - hop_) * sizeof(float));
    }

    int n_ = 0, hop_ = 0, pos_ = 0;
    Fft<float> fft_;
    std::vector<float> win_, inL_, inR_, tL_, tR_, accL_, accR_, outFifoL_, outFifoR_;
    std::vector<float> reL_, imL_, reR_, imR_;
};

/// Primary/ambient decomposition (after Avendano & Jot 2004).  Per time-frequency bin, the
/// inter-channel coherence 2 Re(L R*) / (|L|^2 + |R|^2) measures how "centred" (equal and
/// in phase) the content is; a smoothed mask extracts the centre C = m (L+R)/2 and leaves the
/// ambience L-C, R-C.  Output = g_c C + g_a (X - C), which is bit-exact when both gains are
/// 0 dB.  Uses: vocal/dialogue boost, karaoke (centre removal), ambience enhancement.
class CentreExtractor {
public:
    struct Settings {
        float centreDb = 0.0f, ambienceDb = 0.0f, lowHz = 120.0f, highHz = 10000.0f, focus = 0.5f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    int latency() const { return stft_.latency(); }

private:
    Settings s_;
    double fs_ = 48000.0;
    StftProcessor stft_;
    std::vector<float> mask_;
    int k0_ = 0, k1_ = 0;
    float gc_ = 1.0f, ga_ = 1.0f, gamma_ = 2.0f;
};

/// Spectral noise reduction for hiss and steady background noise: noise power tracked per bin
/// by minimum tracking with slow rise (after Martin 2001), decision-directed a-priori SNR and
/// Wiener gain (Ephraim & Malah 1984) with a gain floor of -amount dB.  A tonal guard caps
/// each bin's noise estimate at the local spectral minimum, so sustained notes survive.  One
/// gain per bin is shared by both channels so the stereo image is preserved.
class SpectralDenoiser {
public:
    struct Settings {
        float amountDb = 12.0f, sensitivity = 0.5f;
    };
    void prepare(double sampleRate);
    void reset();
    void set(const Settings& s);
    void process(float* left, float* right, int n);
    int latency() const { return stft_.latency(); }

private:
    Settings s_;
    double fs_ = 48000.0;
    StftProcessor stft_;
    std::vector<float> smooth_, noise_, gainPrev_, gammaPrev_, gain_, eff_;
    int guard_ = 4;
    int frames_ = 0;
    float floor_ = 0.25f, beta_ = 1.0f, rise_ = 1.0005f;
};

}  // namespace rfx
