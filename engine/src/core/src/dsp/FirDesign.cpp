// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/dsp/FirDesign.h"

#include <algorithm>
#include <cmath>

#include "rfx/dsp/Common.h"
#include "rfx/dsp/Fft.h"

namespace rfx {

double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 64; ++k) {
        term *= q / (static_cast<double>(k) * k);
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

double kaiserWindow(double x, double beta) {
    if (x <= -1.0 || x >= 1.0) return 0.0;
    return besselI0(beta * std::sqrt(1.0 - x * x)) / besselI0(beta);
}

static int nextPow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

std::vector<float> designFirFromCurve(const std::vector<CurvePoint>& points, double fs, int taps,
                                      bool minimumPhase) {
    taps = std::max(16, taps);
    const int M = nextPow2(std::max(8 * taps, 16384));
    const int bins = M / 2 + 1;
    std::vector<double> xs, ys;
    for (const auto& p : points) {
        if (p.freq <= 0.0 || !std::isfinite(p.freq) || !std::isfinite(p.gainDb)) continue;
        if (!xs.empty() && p.freq <= xs.back()) continue;  // require ascending frequencies
        xs.push_back(p.freq);
        ys.push_back(clampv(p.gainDb, -60.0, 40.0));
    }
    std::vector<double> mag(bins, 1.0);
    if (!xs.empty()) {
        for (int k = 0; k < bins; ++k) {
            const double f = std::max(1.0, k * fs / M);
            mag[k] = std::pow(10.0, interpLogX(xs.data(), ys.data(), static_cast<int>(xs.size()), f) / 20.0);
        }
    }
    return designFirFromMagnitude(mag, taps, minimumPhase);
}

std::vector<float> designFirFromMagnitude(const std::vector<double>& mag, int taps, bool minimumPhase) {
    const int bins = static_cast<int>(mag.size());
    const int M = 2 * (bins - 1);
    Fft<double> fft(M);
    std::vector<double> re(bins), im(bins), h(M);
    std::vector<float> out(taps, 0.0f);

    if (!minimumPhase) {
        const double delay = (taps - 1) * 0.5;
        for (int k = 0; k < bins; ++k) {
            const double ph = -kTwoPi * k * delay / M;
            re[k] = mag[k] * std::cos(ph);
            im[k] = mag[k] * std::sin(ph);
        }
        fft.inverse(re.data(), im.data(), h.data());
        const double beta = 6.0;
        for (int n = 0; n < taps && n < M; ++n) {
            const double x = (n - delay) / (delay + 1.0);
            out[n] = static_cast<float>(h[n] * kaiserWindow(x, beta));
        }
        return out;
    }

    // Real cepstrum of the log magnitude.
    for (int k = 0; k < bins; ++k) {
        re[k] = std::log(std::max(mag[k], 1e-9));
        im[k] = 0.0;
    }
    std::vector<double> c(M);
    fft.inverse(re.data(), im.data(), c.data());
    // Fold onto the causal part -> minimum phase.
    for (int n = 1; n < M / 2; ++n) c[n] *= 2.0;
    for (int n = M / 2 + 1; n < M; ++n) c[n] = 0.0;
    fft.forward(c.data(), re.data(), im.data());
    for (int k = 0; k < bins; ++k) {
        const double e = std::exp(re[k]);
        const double ph = im[k];
        re[k] = e * std::cos(ph);
        im[k] = e * std::sin(ph);
    }
    fft.inverse(re.data(), im.data(), h.data());
    const int fadeStart = taps - taps / 4;
    for (int n = 0; n < taps && n < M; ++n) {
        double w = 1.0;
        if (n >= fadeStart) {
            const double t = static_cast<double>(n - fadeStart) / (taps - fadeStart);
            w = 0.5 * (1.0 + std::cos(kPi * t));
        }
        out[n] = static_cast<float>(h[n] * w);
    }
    return out;
}

std::vector<float> resampleOffline(const float* in, int frames, int channels, double srcRate, double dstRate,
                                   int* outFrames) {
    if (srcRate <= 0 || dstRate <= 0 || frames <= 0 || channels <= 0) {
        if (outFrames) *outFrames = 0;
        return {};
    }
    if (std::fabs(srcRate - dstRate) < 1e-6) {
        if (outFrames) *outFrames = frames;
        return std::vector<float>(in, in + static_cast<size_t>(frames) * channels);
    }
    const double ratio = dstRate / srcRate;
    const int nOut = static_cast<int>(std::ceil(frames * ratio));
    const double fc = std::min(1.0, ratio) * 0.95;  // cutoff relative to source Nyquist
    const double halfWidth = 32.0 / fc;              // in source samples
    const double beta = 9.0;
    std::vector<float> out(static_cast<size_t>(nOut) * channels, 0.0f);
    for (int j = 0; j < nOut; ++j) {
        const double t = j / ratio;
        const int i0 = static_cast<int>(std::ceil(t - halfWidth));
        const int i1 = static_cast<int>(std::floor(t + halfWidth));
        for (int c = 0; c < channels; ++c) {
            double acc = 0.0;
            for (int i = std::max(0, i0); i <= std::min(frames - 1, i1); ++i) {
                const double x = t - i;
                const double arg = kPi * fc * x;
                const double s = std::fabs(arg) < 1e-12 ? 1.0 : std::sin(arg) / arg;
                acc += in[static_cast<size_t>(i) * channels + c] * fc * s * kaiserWindow(x / halfWidth, beta);
            }
            out[static_cast<size_t>(j) * channels + c] = static_cast<float>(acc);
        }
    }
    if (outFrames) *outFrames = nOut;
    return out;
}

}  // namespace rfx
