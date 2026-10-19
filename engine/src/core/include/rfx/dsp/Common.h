// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Small math helpers shared by every DSP block.

#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__) || defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#define RFX_X86 1
#endif

namespace rfx {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline float dbToGainf(float db) { return std::pow(10.0f, db * 0.05f); }
inline double gainToDb(double g, double floorDb = -150.0) {
    return g > 1e-30 ? std::max(floorDb, 20.0 * std::log10(g)) : floorDb;
}
inline double powerToDb(double p, double floorDb = -150.0) {
    return p > 1e-30 ? std::max(floorDb, 10.0 * std::log10(p)) : floorDb;
}

template <class T>
inline T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// Coefficient of a one-pole smoother reaching 1-1/e of a step after tauSec.
inline double onePoleCoef(double tauSec, double sampleRate) {
    return tauSec <= 0.0 ? 0.0 : std::exp(-1.0 / (tauSec * sampleRate));
}

/// Linear ramp used to de-click gain changes.
struct LinearRamp {
    float value = 1.0f;
    float target = 1.0f;
    float step = 0.0f;
    int remaining = 0;

    void snap(float v) { value = target = v; remaining = 0; step = 0.0f; }
    void setTarget(float t, int samples) {
        if (t == target && remaining == 0 && value == t) return;
        target = t;
        if (samples <= 0) { snap(t); return; }
        remaining = samples;
        step = (target - value) / static_cast<float>(samples);
    }
    bool ramping() const { return remaining > 0; }
    inline float next() {
        if (remaining > 0) {
            value += step;
            if (--remaining == 0) value = target;
        }
        return value;
    }
};

/// Flushes denormals to zero for the lifetime of the object (real-time thread hygiene).
class ScopedNoDenormals {
public:
    ScopedNoDenormals() {
#if defined(RFX_X86)
        saved_ = _mm_getcsr();
        _mm_setcsr(saved_ | 0x8040);  // FTZ | DAZ
#elif defined(__aarch64__)
        uint64_t fpcr;
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
        saved_ = fpcr;
        fpcr |= (1ull << 24);  // FZ
        __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__arm__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
        uint32_t fpscr;
        __asm__ __volatile__("vmrs %0, fpscr" : "=r"(fpscr));
        saved_ = fpscr;
        fpscr |= (1u << 24);  // FZ
        __asm__ __volatile__("vmsr fpscr, %0" : : "r"(fpscr));
#endif
    }
    ~ScopedNoDenormals() {
#if defined(RFX_X86)
        _mm_setcsr(static_cast<unsigned>(saved_));
#elif defined(__aarch64__)
        uint64_t v = saved_;
        __asm__ __volatile__("msr fpcr, %0" : : "r"(v));
#elif defined(__arm__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
        uint32_t v = static_cast<uint32_t>(saved_);
        __asm__ __volatile__("vmsr fpscr, %0" : : "r"(v));
#endif
    }
    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
    uint64_t saved_ = 0;
};

/// Interpolates y(x) on a curve given as ascending (x, y) pairs, with log-x interpolation
/// and constant extrapolation.  Used for target curves and audiograms.
inline double interpLogX(const double* xs, const double* ys, int n, double x) {
    if (n <= 0) return 0.0;
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n - 1]) return ys[n - 1];
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (xs[mid] <= x) lo = mid; else hi = mid;
    }
    double t = (std::log(x) - std::log(xs[lo])) / (std::log(xs[hi]) - std::log(xs[lo]));
    return ys[lo] + t * (ys[hi] - ys[lo]);
}

}  // namespace rfx
