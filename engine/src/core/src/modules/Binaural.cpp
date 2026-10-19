// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Binaural.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "rfx/data/Kemar.h"
#include "rfx/dsp/Fft.h"
#include "rfx/dsp/FirDesign.h"

namespace rfx {

namespace {
int nextPow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
const float kBaseAz[4] = {-1.0f, 1.0f, -110.0f, 110.0f};  // speakers use +/-angle
}  // namespace

void HrtfSpatializer::prepare(double fs) {
    fs_ = fs;
    taps_ = fs <= 50000.0 ? 128 : (fs <= 100000.0 ? 256 : 512);
    const int M = 2048;
    const int bins = M / 2 + 1;
    Fft<double> fft(M);
    std::vector<double> buf(M), re(bins), im(bins);
    std::vector<std::vector<double>> mags(kGrid * 2, std::vector<double>(bins));
    std::vector<double> avg(bins, 0.0);

    // Resample, measure onset delay and magnitude of every measured response.
    for (int a = 0; a < kGrid; ++a) {
        for (int e = 0; e < 2; ++e) {
            int frames = 0;
            std::vector<float> h = resampleOffline(kemar::kHrir[a][e], kemar::kTaps, 1, kemar::kSampleRate, fs, &frames);
            float peak = 0.0f;
            for (float v : h) peak = std::max(peak, std::fabs(v));
            int onset = 0;
            while (onset < frames && std::fabs(h[onset]) < 0.2f * peak) ++onset;
            delays_[a][e] = std::max(0, onset - 1);
            std::fill(buf.begin(), buf.end(), 0.0);
            for (int k = 0; k < frames && k < M; ++k) buf[k] = h[k];
            fft.forward(buf.data(), re.data(), im.data());
            auto& mg = mags[a * 2 + e];
            for (int k = 0; k < bins; ++k) {
                mg[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
                avg[k] += mg[k] * mg[k];
            }
        }
    }
    // Diffuse-field equalisation: invert the (1/3-octave smoothed) average power response.
    for (auto& v : avg) v /= kGrid * 2.0;
    std::vector<double> eq(bins, 1.0);
    const double binHz = fs / M;
    for (int k = 1; k < bins; ++k) {
        const double f = std::max(k * binHz, 120.0);
        const int lo = std::max(1, static_cast<int>(f / std::pow(2.0, 1.0 / 6.0) / binHz));
        const int hi = std::min(bins - 1, static_cast<int>(f * std::pow(2.0, 1.0 / 6.0) / binHz) + 1);
        double s = 0.0;
        for (int j = lo; j <= hi; ++j) s += avg[j];
        eq[k] = 1.0 / std::sqrt(std::max(s / (hi - lo + 1), 1e-12));
    }
    eq[0] = eq[1];
    const int k1k = static_cast<int>(1000.0 / binHz);
    const double ref = eq[k1k];
    for (auto& v : eq) v = clampv(v / ref, dbToGain(-15.0), dbToGain(15.0)) * ref;

    // Minimum-phase filters from the equalised magnitudes; store reversed for fast dot products.
    table_.assign(static_cast<size_t>(kGrid) * 2 * taps_, 0.0f);
    std::vector<double> mag(bins);
    for (int a = 0; a < kGrid; ++a) {
        for (int e = 0; e < 2; ++e) {
            const auto& mg = mags[a * 2 + e];
            for (int k = 0; k < bins; ++k) mag[k] = mg[k] * eq[k];
            std::vector<float> h = designFirFromMagnitude(mag, taps_, true);
            float* dst = table_.data() + (static_cast<size_t>(a) * 2 + e) * taps_;
            for (int k = 0; k < taps_; ++k) dst[k] = h[taps_ - 1 - k];
        }
    }

    // Normalise so a centred (mono) signal through the default +/-30 degree pair is 0 dB at 1 kHz.
    {
        Filter f30, fm30;
        interpolate(30.0f, f30);
        interpolate(-30.0f, fm30);
        const double w = kTwoPi * 1000.0 / fs;
        std::complex<double> sum = 0.0;
        for (const Filter* f : {&f30, &fm30}) {
            for (int k = 0; k < taps_; ++k)
                sum += static_cast<double>(f->h[0][taps_ - 1 - k]) * std::polar(1.0, -w * (k + f->delay[0]));
        }
        norm_ = static_cast<float>(clampv(1.0 / std::max(std::abs(sum), 1e-6), 0.1, 2.0));
    }

    const int maxDelay = static_cast<int>(0.012 * fs) + 2 * taps_ + 64;
    const int size = nextPow2(maxDelay);
    histMask_ = size - 1;
    for (auto& h : hist_) h.assign(2 * size, 0.0f);
    // Pre-size every filter so the audio thread never allocates.
    for (int s = 0; s < kSources; ++s) {
        for (int e = 0; e < 2; ++e) {
            cur_[s].h[e].assign(taps_, 0.0f);
            next_[s].h[e].assign(taps_, 0.0f);
        }
    }
    reset();
}

void HrtfSpatializer::reset() {
    for (auto& h : hist_) std::fill(h.begin(), h.end(), 0.0f);
    t_ = 0;
    for (int s = 0; s < kSources; ++s) cur_[s].az = 1e9f;
}

void HrtfSpatializer::interpolate(float az, Filter& out) const {
    while (az > 180.0f) az -= 360.0f;
    while (az <= -180.0f) az += 360.0f;
    const bool mirror = az < 0.0f;
    const float a = std::fabs(az) / 5.0f;
    const int i0 = std::min(static_cast<int>(a), kGrid - 1);
    const int i1 = std::min(i0 + 1, kGrid - 1);
    const float t = a - i0;
    for (int ear = 0; ear < 2; ++ear) {
        const int src = mirror ? 1 - ear : ear;
        const float* h0 = table_.data() + (static_cast<size_t>(i0) * 2 + src) * taps_;
        const float* h1 = table_.data() + (static_cast<size_t>(i1) * 2 + src) * taps_;
        out.h[ear].resize(taps_);
        for (int k = 0; k < taps_; ++k) out.h[ear][k] = h0[k] + t * (h1[k] - h0[k]);
        out.delay[ear] = static_cast<int>(std::lround(delays_[i0][src] + t * (delays_[i1][src] - delays_[i0][src])));
    }
    out.az = az;
}

void HrtfSpatializer::response(float az, std::vector<float>& l, std::vector<float>& r, float* dl, float* dr) const {
    Filter f;
    interpolate(az, f);
    l.assign(f.h[0].rbegin(), f.h[0].rend());
    r.assign(f.h[1].rbegin(), f.h[1].rend());
    if (dl) *dl = static_cast<float>(f.delay[0]);
    if (dr) *dr = static_cast<float>(f.delay[1]);
}

inline double HrtfSpatializer::renderSample(const Filter& f, int src, int ear, int64_t t) const {
    const int extra = src == 2 ? static_cast<int>(0.007 * fs_) : (src == 3 ? static_cast<int>(0.011 * fs_) : 0);
    const int64_t start = t - extra - f.delay[ear] - (taps_ - 1);
    // Reflections only need the head-shadow shape: the first quarter of the minimum-phase
    // response (the tail of the reversed array) is enough at a quarter of the cost.
    const int len = src >= 2 ? taps_ / 4 : taps_;
    const int skip = taps_ - len;
    const float* x = hist_[src].data() + (start & histMask_) + skip;
    const float* h = f.h[ear].data() + skip;
    // Eight independent float accumulators: lets the compiler use SIMD without -ffast-math.
    // taps_ (and taps_ / 4) is always a multiple of 8.
    float a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0;
    for (int k = 0; k < len; k += 8) {
        a0 += h[k] * x[k];
        a1 += h[k + 1] * x[k + 1];
        a2 += h[k + 2] * x[k + 2];
        a3 += h[k + 3] * x[k + 3];
        a4 += h[k + 4] * x[k + 4];
        a5 += h[k + 5] * x[k + 5];
        a6 += h[k + 6] * x[k + 6];
        a7 += h[k + 7] * x[k + 7];
    }
    return static_cast<double>(((a0 + a1) + (a2 + a3)) + ((a4 + a5) + (a6 + a7)));
}

void HrtfSpatializer::process(float* left, float* right, int n) {
    const float yaw = s_.tracking ? s_.yawDeg : 0.0f;
    const float angle = clampv(s_.angleDeg, 5.0f, 90.0f);
    const int sources = s_.room > 0.001f ? 4 : 2;
    const float refl = 0.35f * clampv(s_.room, 0.0f, 1.0f);
    const int size = histMask_ + 1;
    for (int off = 0; off < n; off += kBlock) {
        const int m = std::min(kBlock, n - off);
        bool change[kSources] = {false, false, false, false};
        for (int s = 0; s < sources; ++s) {
            const float base = s < 2 ? kBaseAz[s] * angle : kBaseAz[s];
            const float az = base - yaw;
            if (std::fabs(az - cur_[s].az) > 0.2f) {
                interpolate(az, next_[s]);
                change[s] = cur_[s].az < 1e8f;  // crossfade except on the very first block
                if (!change[s]) cur_[s] = next_[s];
            }
        }
        for (int i = 0; i < m; ++i) {
            const float in[2] = {left[off + i], right[off + i]};
            const int64_t t = t_;
            const int w = static_cast<int>(t & histMask_);
            for (int s = 0; s < sources; ++s) {
                hist_[s][w] = in[s & 1];
                hist_[s][w + size] = in[s & 1];
            }
            const float fade = static_cast<float>(i + 1) / m;
            double out[2] = {0.0, 0.0};
            for (int s = 0; s < sources; ++s) {
                const double g = s < 2 ? 1.0 : refl;
                for (int ear = 0; ear < 2; ++ear) {
                    double y = renderSample(cur_[s], s, ear, t);
                    if (change[s]) y += (renderSample(next_[s], s, ear, t) - y) * fade;
                    out[ear] += g * y;
                }
            }
            left[off + i] = static_cast<float>(out[0] * norm_);
            right[off + i] = static_cast<float>(out[1] * norm_);
            ++t_;
        }
        for (int s = 0; s < sources; ++s)
            if (change[s]) cur_[s] = next_[s];
    }
}

}  // namespace rfx
