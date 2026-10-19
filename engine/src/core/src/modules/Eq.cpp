// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Eq.h"

#include <cmath>
#include <cstring>

namespace rfx {

namespace {
constexpr int kSubBlock = 32;
constexpr double kGlideSeconds = 0.02;

EqBand sanitize(EqBand b, double fs) {
    int t = static_cast<int>(b.type);
    if (t < 0 || t >= static_cast<int>(FilterType::Count)) b.type = FilterType::Peak;
    if (!std::isfinite(b.freq)) b.freq = 1000.0f;
    if (!std::isfinite(b.gainDb)) b.gainDb = 0.0f;
    if (!std::isfinite(b.q)) b.q = 0.7071f;
    b.freq = static_cast<float>(clampv<double>(b.freq, 10.0, 0.49 * fs));
    b.gainDb = clampv(b.gainDb, -30.0f, 30.0f);
    b.q = clampv(b.q, 0.05f, 40.0f);
    if (b.channel < 0 || b.channel > 4) b.channel = 0;
    return b;
}
}  // namespace

int decodeBands(const float* v, int count, EqBand* out, int maxBands) {
    const int n = std::min(count / 6, maxBands);
    for (int i = 0; i < n; ++i) {
        const float* p = v + i * 6;
        EqBand b;
        b.enabled = p[0] >= 0.5f;
        b.type = static_cast<FilterType>(clampv(static_cast<int>(std::lround(p[1])), 0,
                                                static_cast<int>(FilterType::Count) - 1));
        b.freq = p[2];
        b.gainDb = p[3];
        b.q = p[4];
        b.channel = clampv(static_cast<int>(std::lround(p[5])), 0, 4);
        out[i] = b;
    }
    return n;
}

ParametricEq::ParametricEq(int capacity) : slots_(static_cast<size_t>(capacity)) {
    prepare(48000.0);
}

void ParametricEq::prepare(double sampleRate) {
    fs_ = sampleRate;
    smoothK_ = 1.0 - std::exp(-static_cast<double>(kSubBlock) / (kGlideSeconds * fs_));
    for (auto& s : slots_) {
        s.target = sanitize(s.target, fs_);
        s.freq = static_cast<float>(clampv<double>(s.freq, 10.0, 0.49 * fs_));
        updateCoeffs(s);
    }
    reset();
}

void ParametricEq::reset() {
    for (auto& s : slots_) {
        s.sL.reset();
        s.sR.reset();
        if (s.smoothing) {
            s.freq = s.target.freq;
            s.gain = s.target.gainDb;
            s.q = s.target.q;
            s.smoothing = false;
            if (s.fadingOut) {
                s.active = false;
                s.fadingOut = false;
            }
            updateCoeffs(s);
        }
    }
    preamp_.snap(preamp_.target);
}

void ParametricEq::setMatched(bool matched) {
    if (matched == matched_) return;
    matched_ = matched;
    for (auto& s : slots_)
        if (s.active) updateCoeffs(s);
}

void ParametricEq::setPreampDb(float db) {
    preamp_.setTarget(dbToGainf(db), static_cast<int>(0.02 * fs_));
}

void ParametricEq::updateCoeffs(Slot& s) {
    s.c = designFilter(s.type, s.freq, s.gain, s.q, fs_, matched_);
}

void ParametricEq::setBands(const EqBand* bands, int n) {
    const int cap = capacity();
    for (int i = 0; i < cap; ++i) {
        EqBand nb = i < n ? sanitize(bands[i], fs_) : EqBand{};
        Slot& s = slots_[i];
        if (!s.active) {
            s.target = nb;
            if (!nb.enabled) continue;
            s.active = true;
            s.fadingOut = false;
            s.type = nb.type;
            s.channel = nb.channel;
            s.freq = nb.freq;
            s.q = nb.q;
            const bool gainType = filterHasGain(nb.type);
            s.gain = gainType ? 0.0f : nb.gainDb;
            s.smoothing = gainType && nb.gainDb != 0.0f;
            s.sL.reset();
            s.sR.reset();
            updateCoeffs(s);
            continue;
        }
        if (!nb.enabled) {
            if (filterHasGain(s.type)) {
                s.target.gainDb = 0.0f;
                s.target.enabled = false;
                s.fadingOut = true;
                s.smoothing = true;
            } else {
                s.active = false;
                s.target = nb;
            }
            continue;
        }
        if (nb.type != s.type || nb.channel != s.channel) {
            if (nb.channel != s.channel) {
                s.sL.reset();
                s.sR.reset();
            }
            s.type = nb.type;
            s.channel = nb.channel;
            s.freq = nb.freq;
            s.gain = nb.gainDb;
            s.q = nb.q;
            s.target = nb;
            s.smoothing = false;
            s.fadingOut = false;
            updateCoeffs(s);
            continue;
        }
        s.target = nb;
        s.fadingOut = false;
        s.smoothing = s.freq != nb.freq || s.gain != nb.gainDb || s.q != nb.q;
    }
}

void ParametricEq::stepSmoothing(Slot& s) {
    const double k = smoothK_;
    const double lf = std::log(static_cast<double>(s.freq)), tlf = std::log(static_cast<double>(s.target.freq));
    const double lq = std::log(static_cast<double>(s.q)), tlq = std::log(static_cast<double>(s.target.q));
    const double nlf = lf + k * (tlf - lf);
    const double nlq = lq + k * (tlq - lq);
    const double ng = s.gain + k * (s.target.gainDb - s.gain);
    if (std::fabs(tlf - nlf) < 1e-4 && std::fabs(tlq - nlq) < 1e-4 && std::fabs(s.target.gainDb - ng) < 0.005) {
        s.freq = s.target.freq;
        s.q = s.target.q;
        s.gain = s.target.gainDb;
        s.smoothing = false;
        if (s.fadingOut) {
            s.active = false;
            s.fadingOut = false;
            s.sL.reset();
            s.sR.reset();
            return;
        }
    } else {
        s.freq = static_cast<float>(std::exp(nlf));
        s.q = static_cast<float>(std::exp(nlq));
        s.gain = static_cast<float>(ng);
    }
    updateCoeffs(s);
}

bool ParametricEq::idle() const {
    if (preamp_.ramping() || preamp_.value != 1.0f) return false;
    for (const auto& s : slots_)
        if (s.active) return false;
    return true;
}

void ParametricEq::process(float* left, float* right, int n) {
    for (int off = 0; off < n; off += kSubBlock) {
        const int m = std::min(kSubBlock, n - off);
        float* L = left + off;
        float* R = right + off;
        bool midSide = false;
        for (auto& s : slots_) {
            if (!s.active) continue;
            if (s.smoothing) {
                stepSmoothing(s);
                if (!s.active) continue;
            }
            if (s.channel >= 3) {
                midSide = true;
                continue;
            }
            if (s.channel != 2)
                for (int i = 0; i < m; ++i) L[i] = static_cast<float>(biquadTick(s.c, s.sL, L[i]));
            if (s.channel != 1)
                for (int i = 0; i < m; ++i) R[i] = static_cast<float>(biquadTick(s.c, s.sR, R[i]));
        }
        if (midSide) {
            // Mid (3) and side (4) bands run on M = (L+R)/2 and S = (L-R)/2.
            for (int i = 0; i < m; ++i) {
                double mid = 0.5 * (static_cast<double>(L[i]) + R[i]);
                double side = 0.5 * (static_cast<double>(L[i]) - R[i]);
                for (auto& s : slots_) {
                    if (!s.active || s.channel < 3) continue;
                    if (s.channel == 3) mid = biquadTick(s.c, s.sL, mid);
                    else side = biquadTick(s.c, s.sR, side);
                }
                L[i] = static_cast<float>(mid + side);
                R[i] = static_cast<float>(mid - side);
            }
        }
        if (preamp_.ramping()) {
            for (int i = 0; i < m; ++i) {
                const float g = preamp_.next();
                L[i] *= g;
                R[i] *= g;
            }
        } else if (preamp_.value != 1.0f) {
            const float g = preamp_.value;
            for (int i = 0; i < m; ++i) {
                L[i] *= g;
                R[i] *= g;
            }
        }
    }
}

double ParametricEq::responseDb(const EqBand* bands, int n, bool matched, double freq, double fs) {
    double db = 0.0;
    for (int i = 0; i < n; ++i) {
        if (!bands[i].enabled) continue;
        const EqBand b = sanitize(bands[i], fs);
        db += designFilter(b.type, b.freq, b.gainDb, b.q, fs, matched).magnitudeDb(freq, fs);
    }
    return db;
}

// ---------------------------------------------------------------------------
namespace geq {

static const double kOct[] = {31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
static const double kTwoThirds[] = {25, 40, 63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000, 16000};
static const double kThird[] = {20,   25,   31.5, 40,   50,   63,   80,    100,   125,   160,  200,
                                250,  315,  400,  500,  630,  800,  1000,  1250,  1600,  2000, 2500,
                                3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};

int bandCount(int layout) {
    switch (layout) {
        case kTwoThirds15: return 15;
        case kThird31: return 31;
        default: return 10;
    }
}

const double* centers(int layout) {
    switch (layout) {
        case kTwoThirds15: return kTwoThirds;
        case kThird31: return kThird;
        default: return kOct;
    }
}

double bandQ(int layout) {
    switch (layout) {
        case kTwoThirds15: return 1.41;
        case kThird31: return 2.8;
        default: return 1.0;
    }
}

}  // namespace geq

namespace {

// Solves (A^T A + lambda I) x = A^T b for small dense systems (rows x cols, row-major A).
void leastSquares(const std::vector<double>& A, const std::vector<double>& b, int rows, int cols, double lambda,
                  std::vector<double>& x) {
    std::vector<double> M(static_cast<size_t>(cols) * cols, 0.0), v(cols, 0.0);
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < cols; ++j) {
            double s = 0.0;
            for (int r = 0; r < rows; ++r) s += A[r * cols + i] * A[r * cols + j];
            M[i * cols + j] = s + (i == j ? lambda : 0.0);
        }
        double s = 0.0;
        for (int r = 0; r < rows; ++r) s += A[r * cols + i] * b[r];
        v[i] = s;
    }
    // Gaussian elimination with partial pivoting.
    for (int c = 0; c < cols; ++c) {
        int piv = c;
        for (int r = c + 1; r < cols; ++r)
            if (std::fabs(M[r * cols + c]) > std::fabs(M[piv * cols + c])) piv = r;
        if (piv != c) {
            for (int k = 0; k < cols; ++k) std::swap(M[c * cols + k], M[piv * cols + k]);
            std::swap(v[c], v[piv]);
        }
        const double d = M[c * cols + c];
        if (std::fabs(d) < 1e-15) continue;
        for (int r = c + 1; r < cols; ++r) {
            const double f = M[r * cols + c] / d;
            if (f == 0.0) continue;
            for (int k = c; k < cols; ++k) M[r * cols + k] -= f * M[c * cols + k];
            v[r] -= f * v[c];
        }
    }
    x.assign(cols, 0.0);
    for (int c = cols - 1; c >= 0; --c) {
        double s = v[c];
        for (int k = c + 1; k < cols; ++k) s -= M[c * cols + k] * x[k];
        const double d = M[c * cols + c];
        x[c] = std::fabs(d) < 1e-15 ? 0.0 : s / d;
    }
}

}  // namespace

void fitPeakingGains(const double* centersHz, const double* targetDb, int count, double q, double fs,
                     double* outGains, double maxGainDb, double centreWeight, bool matched) {
    std::vector<int> valid;
    for (int k = 0; k < count; ++k) {
        outGains[k] = 0.0;
        if (centersHz[k] < 0.45 * fs) valid.push_back(k);
    }
    const int K = static_cast<int>(valid.size());
    if (K == 0) return;

    // Design points: band centres and geometric midpoints between them.
    std::vector<double> df, dt, dw;
    for (int j = 0; j < K; ++j) {
        df.push_back(centersHz[valid[j]]);
        dt.push_back(targetDb[valid[j]] * centreWeight);
        dw.push_back(centreWeight);
        if (j + 1 < K) {
            df.push_back(std::sqrt(centersHz[valid[j]] * centersHz[valid[j + 1]]));
            dt.push_back(0.5 * (targetDb[valid[j]] + targetDb[valid[j + 1]]));
            dw.push_back(1.0);
        }
    }
    const int D = static_cast<int>(df.size());
    std::vector<double> A(static_cast<size_t>(D) * K), g(K, 0.0);
    const double proto = 12.0;
    for (int pass = 0; pass < 3; ++pass) {
        for (int j = 0; j < K; ++j) {
            double gj = pass == 0 ? proto : g[j];
            if (std::fabs(gj) < 0.5) gj = gj < 0.0 ? -0.5 : 0.5;
            const BiquadCoeffs c = designFilter(FilterType::Peak, centersHz[valid[j]], gj, q, fs, matched);
            for (int d = 0; d < D; ++d) A[d * K + j] = dw[d] * c.magnitudeDb(df[d], fs) / gj;
        }
        leastSquares(A, dt, D, K, 1e-4, g);
        for (auto& x : g) x = clampv(x, -maxGainDb, maxGainDb);
    }
    for (int j = 0; j < K; ++j) outGains[valid[j]] = g[j];
}

int designGraphicEq(int layout, const float* sliderDb, int sliders, double fs, EqBand* out, int maxBands) {
    const int n = geq::bandCount(layout);
    const double* c = geq::centers(layout);
    std::vector<double> t(n, 0.0), gains(n, 0.0);
    bool any = false;
    for (int i = 0; i < n; ++i) {
        t[i] = i < sliders ? clampv<double>(sliderDb[i], -15.0, 15.0) : 0.0;
        if (std::fabs(t[i]) > 1e-3) any = true;
    }
    if (any) fitPeakingGains(c, t.data(), n, geq::bandQ(layout), fs, gains.data());
    const int count = std::min(n, maxBands);
    for (int i = 0; i < count; ++i) {
        EqBand b;
        b.type = FilterType::Peak;
        b.freq = static_cast<float>(c[i]);
        b.q = static_cast<float>(geq::bandQ(layout));
        b.gainDb = static_cast<float>(gains[i]);
        b.enabled = std::fabs(gains[i]) > 0.01;
        b.channel = 0;
        out[i] = b;
    }
    return count;
}

}  // namespace rfx
