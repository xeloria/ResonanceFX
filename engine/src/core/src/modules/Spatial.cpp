// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Spatial.h"

#include <cmath>

namespace rfx {

// ---------------------------------------------------------------------------
// StereoImage
// ---------------------------------------------------------------------------
void StereoImage::prepare(double fs) {
    fs_ = fs;
    int size = 1;
    while (size < static_cast<int>(0.031 * fs) + 2) size <<= 1;
    decorBuf_.assign(size, 0.0f);
    haasBuf_.assign(size, 0.0f);
    decorMask_ = haasMask_ = size - 1;
    // Lauridsen pseudo-stereo: ~12 ms keeps the comb teeth dense and the effect spacious.
    decorDelay_ = static_cast<int>(0.012 * fs);
    decorHp_ = designRbj(FilterType::HighPass, 200.0, 0.0, 0.7071, fs_);
    // Re-apply the current settings with every sample-rate-dependent design forced.
    const Settings keep = s_;
    s_.bassMonoHz = -1.0f;
    s_.lowXover = -1.0f;
    set(keep);
    reset();
}

void StereoImage::reset() {
    s1_.reset();
    s2_.reset();
    dh1_.reset();
    dh2_.reset();
    xo_.reset();
    std::fill(decorBuf_.begin(), decorBuf_.end(), 0.0f);
    std::fill(haasBuf_.begin(), haasBuf_.end(), 0.0f);
    pos_ = 0;
    width_.snap(s_.width);
    decor_.snap(s_.decorrelate);
}

void StereoImage::set(const Settings& s) {
    const bool redesign = s.bassMonoHz != s_.bassMonoHz;
    const bool xover = s.lowXover != s_.lowXover || s.highXover != s_.highXover;
    s_ = s;
    s_.width = clampv(s_.width, 0.0f, 3.0f);
    width_.setTarget(s_.width, static_cast<int>(0.03 * fs_));
    decor_.setTarget(clampv(s_.decorrelate, 0.0f, 1.0f), static_cast<int>(0.03 * fs_));
    if (redesign) {
        hp_ = s_.bassMonoHz > 0.0f ? designRbj(FilterType::HighPass, clampv(s_.bassMonoHz, 20.0f, 1000.0f), 0.0,
                                              0.7071, fs_)
                                   : BiquadCoeffs{};
    }
    const bool wasBanded = bandWidths_;
    bandWidths_ = s_.widthLow != 1.0f || s_.widthMid != 1.0f || s_.widthHigh != 1.0f;
    if (xover || (bandWidths_ && !wasBanded)) {
        const double f[2] = {clampv(static_cast<double>(s_.lowXover), 40.0, 2000.0),
                             clampv(static_cast<double>(s_.highXover), 500.0, 0.4 * fs_)};
        xo_.configure(f, 2, fs_);
    }
    if (!haasBuf_.empty()) haasDelay_ = clampv(static_cast<int>(std::lround(std::fabs(s_.haasMs) * 1e-3 * fs_)), 0, haasMask_);
}

void StereoImage::process(float* left, float* right, int n) {
    const bool mono = s_.bassMonoHz > 0.0f;
    const bool decor = decor_.value > 0.0f || decor_.ramping();
    const bool haas = haasDelay_ > 0;
    double bl[3], br[3];
    for (int i = 0; i < n; ++i) {
        const double m = 0.5 * (left[i] + right[i]);
        double s = 0.5 * (left[i] - right[i]);
        if (bandWidths_) {
            xo_.split(s, 0.0, bl, br);
            s = s_.widthLow * bl[0] + s_.widthMid * bl[1] + s_.widthHigh * bl[2];
        }
        if (mono) s = biquadTick(hp_, s2_, biquadTick(hp_, s1_, s));  // LR4 high-pass on side
        s *= width_.next();
        const float dAmt = decor_.next();
        if (decor) {
            // Complementary combs from the (bass-free) mid: L gets +d, R gets -d, so L+R is unchanged.
            const double mh = biquadTick(decorHp_, dh2_, biquadTick(decorHp_, dh1_, m));
            decorBuf_[pos_ & decorMask_] = static_cast<float>(mh);
            s += 0.7 * dAmt * decorBuf_[(pos_ - decorDelay_) & decorMask_];
        }
        float l = static_cast<float>(m + s);
        float r = static_cast<float>(m - s);
        if (haas) {
            float& delayed = s_.haasMs > 0.0f ? r : l;
            haasBuf_[pos_ & haasMask_] = delayed;
            delayed = haasBuf_[(pos_ - haasDelay_) & haasMask_];
        }
        pos_ = (pos_ + 1) & 0x3FFFFFFF;
        left[i] = l;
        right[i] = r;
    }
}

// ---------------------------------------------------------------------------
// Crossfeed
// ---------------------------------------------------------------------------
double Crossfeed::itdSeconds(double angleDeg, double a) {
    const double c = 343.0;
    const double ts = clampv(angleDeg, 0.0, 90.0) * kPi / 180.0;
    const double ti = kPi / 2.0 - ts;  // incidence angle re ear axis, ipsilateral
    const double tc = kPi / 2.0 + ts;  // contralateral
    auto woodworth = [&](double t) { return t < kPi / 2.0 ? -(a / c) * std::cos(t) : (a / c) * (t - kPi / 2.0); };
    return woodworth(tc) - woodworth(ti);
}

void Crossfeed::prepare(double fs) {
    fs_ = fs;
    dL_.assign(512, 0.0f);
    dR_.assign(512, 0.0f);
    design();
    reset();
}

void Crossfeed::reset() {
    loL_ = loR_ = hiL_ = hiR_ = asisL_ = asisR_ = 0.0;
    shL_.reset();
    shR_.reset();
    std::fill(dL_.begin(), dL_.end(), 0.0f);
    std::fill(dR_.begin(), dR_.end(), 0.0f);
    dPos_ = 0;
}

void Crossfeed::set(const Settings& s) {
    s_ = s;
    s_.cutoffHz = clampv(s_.cutoffHz, 200.0f, 3000.0f);
    s_.feedDb = clampv(s_.feedDb, 0.5f, 20.0f);
    s_.angleDeg = clampv(s_.angleDeg, 5.0f, 90.0f);
    s_.amount = clampv(s_.amount, 0.0f, 1.0f);
    s_.headRadiusCm = clampv(s_.headRadiusCm, 5.0f, 12.0f);
    design();
}

void Crossfeed::design() {
    // --- Bauer / bs2b ---
    const double level = s_.feedDb;
    const double gbLo = level * -5.0 / 6.0 - 3.0;
    const double gbHi = level / 6.0 - 3.0;
    const double gLo = std::pow(10.0, gbLo / 20.0);
    const double gHi = 1.0 - std::pow(10.0, gbHi / 20.0);
    const double fcLo = s_.cutoffHz;
    const double fcHi = fcLo * std::pow(2.0, (gbLo - 20.0 * std::log10(gHi)) / 12.0);
    double x = std::exp(-kTwoPi * fcLo / fs_);
    b1lo_ = x;
    a0lo_ = gLo * (1.0 - x);
    x = std::exp(-kTwoPi * std::min(fcHi, 0.45 * fs_) / fs_);
    b1hi_ = x;
    a0hi_ = 1.0 - gHi * (1.0 - x);
    a1hi_ = -x;
    gain_ = 1.0 / (1.0 - gHi + gLo);

    // --- spherical head (Brown & Duda 1998) ---
    const double a = s_.headRadiusCm / 100.0;
    const double w0 = 343.0 / a;
    const double ts = s_.angleDeg;
    auto alpha = [](double incidenceDeg) {
        const double amin = 0.1, tmin = 150.0;
        return (1.0 + amin / 2.0) + (1.0 - amin / 2.0) * std::cos(incidenceDeg / tmin * kPi);
    };
    const double ai = alpha(90.0 - ts), ac = alpha(90.0 + ts);
    // Contralateral / ipsilateral head-shadow ratio: (1 + ac s/2w0) / (1 + ai s/2w0), bilinear.
    const double tz = ac / (2.0 * w0), tp = ai / (2.0 * w0), k = 2.0 * fs_;
    const double a0 = 1.0 + k * tp;
    shadow_.b0 = (1.0 + k * tz) / a0;
    shadow_.b1 = (1.0 - k * tz) / a0;
    shadow_.b2 = 0.0;
    shadow_.a1 = (1.0 - k * tp) / a0;
    shadow_.a2 = 0.0;
    delay_ = clampv(itdSeconds(ts, a) * fs_, 0.0, 500.0);
    amount_ = s_.amount;
    norm_ = 1.0 / (1.0 + amount_);
}

void Crossfeed::process(float* left, float* right, int n) {
    if (s_.mode == 0) {
        for (int i = 0; i < n; ++i) {
            const double l = left[i], r = right[i];
            loL_ = a0lo_ * l + b1lo_ * loL_;
            loR_ = a0lo_ * r + b1lo_ * loR_;
            hiL_ = a0hi_ * l + a1hi_ * asisL_ + b1hi_ * hiL_;
            hiR_ = a0hi_ * r + a1hi_ * asisR_ + b1hi_ * hiR_;
            asisL_ = l;
            asisR_ = r;
            left[i] = static_cast<float>((hiL_ + loR_) * gain_);
            right[i] = static_cast<float>((hiR_ + loL_) * gain_);
        }
        return;
    }
    const int size = static_cast<int>(dL_.size());
    const int di = static_cast<int>(delay_);
    const double frac = delay_ - di;
    for (int i = 0; i < n; ++i) {
        const float l = left[i], r = right[i];
        dL_[dPos_] = l;
        dR_[dPos_] = r;
        int p0 = dPos_ - di;
        if (p0 < 0) p0 += size;
        int p1 = p0 - 1;
        if (p1 < 0) p1 += size;
        const double dl = dL_[p0] * (1.0 - frac) + dL_[p1] * frac;
        const double dr = dR_[p0] * (1.0 - frac) + dR_[p1] * frac;
        if (++dPos_ == size) dPos_ = 0;
        const double xl = biquadTick(shadow_, shL_, dl);  // left source reaching the right ear
        const double xr = biquadTick(shadow_, shR_, dr);  // right source reaching the left ear
        left[i] = static_cast<float>((l + amount_ * xr) * norm_);
        right[i] = static_cast<float>((r + amount_ * xl) * norm_);
    }
}

// ---------------------------------------------------------------------------
// Reverb
// ---------------------------------------------------------------------------
namespace {
int nextPow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
bool isPrime(int v) {
    if (v < 2) return false;
    for (int d = 2; d * d <= v; ++d)
        if (v % d == 0) return false;
    return true;
}
int nearestPrime(int v) {
    for (int d = 0; d < 1000; ++d) {
        if (isPrime(v + d)) return v + d;
        if (v - d > 1 && isPrime(v - d)) return v - d;
    }
    return v;
}
const double kLineMs[8] = {29.7, 37.1, 41.1, 43.7, 53.1, 59.3, 67.1, 73.3};
const double kDiffMs[4] = {4.77, 3.59, 12.73, 9.31};
const double kTapMs[12] = {7.1, 11.3, 15.8, 19.4, 23.9, 28.6, 33.1, 37.7, 43.3, 49.1, 55.7, 62.9};
const float kOutL[8] = {1, -1, 1, -1, 1, -1, 1, -1};
const float kOutR[8] = {1, 1, -1, -1, 1, 1, -1, -1};
const float kIn[8] = {1, -1, 1, 1, -1, 1, -1, -1};
}  // namespace

void Reverb::prepare(double fs) {
    fs_ = fs;
    const int maxLine = static_cast<int>(73.3 * 1.6 * 1e-3 * fs + 0.004 * fs) + 8;
    for (auto& l : lines_) {
        const int size = nextPow2(maxLine + 2);
        l.buf.assign(size, 0.0f);
        l.mask = size - 1;
    }
    for (int i = 0; i < kDiffusers; ++i) {
        diff_[i].len = std::max(1, nearestPrime(static_cast<int>(kDiffMs[i] * 1e-3 * fs)));
        diff_[i].buf.assign(diff_[i].len, 0.0f);
        diff_[i].pos = 0;
        diff_[i].g = 0.62f;
    }
    const int preSize = nextPow2(static_cast<int>(0.2 * fs + 70.0 * 1.5 * 1e-3 * fs) + 16);
    pre_.assign(preSize, 0.0f);
    preMask_ = preSize - 1;
    lfoInc_[0] = 0.71 / fs;
    lfoInc_[1] = 0.97 / fs;
    set(s_);
    for (auto& l : lines_) l.length = l.target;
    reset();
}

void Reverb::reset() {
    for (auto& l : lines_) {
        std::fill(l.buf.begin(), l.buf.end(), 0.0f);
        l.lp = 0.0;
        l.length = l.target;
    }
    for (auto& d : diff_) {
        std::fill(d.buf.begin(), d.buf.end(), 0.0f);
        d.pos = 0;
    }
    std::fill(pre_.begin(), pre_.end(), 0.0f);
    writePos_ = 0;
    inHpS_.reset();
    inLpS_.reset();
    wet_.snap(wet_.target);
    dry_.snap(dry_.target);
}

void Reverb::set(const Settings& s) {
    s_ = s;
    s_.size = clampv(s_.size, 0.0f, 1.0f);
    s_.decayS = clampv(s_.decayS, 0.05f, 30.0f);
    s_.damping = clampv(s_.damping, 0.0f, 1.0f);
    s_.predelayMs = clampv(s_.predelayMs, 0.0f, 200.0f);
    const double scale = 0.4 + 1.2 * s_.size;
    for (int i = 0; i < kLines; ++i)
        lines_[i].target = nearestPrime(static_cast<int>(kLineMs[i] * scale * 1e-3 * fs_));
    preDelay_ = s_.predelayMs * 1e-3 * fs_;
    const double erScale = 0.5 + s_.size;
    for (int k = 0; k < kTaps; ++k) {
        tapDelay_[k] = static_cast<int>(kTapMs[k] * erScale * 1e-3 * fs_);
        const float g = 0.8f * std::pow(0.86f, static_cast<float>(k));
        tapGainL_[k] = (k % 2 == 0) ? g : 0.35f * g;
        tapGainR_[k] = (k % 2 == 0) ? 0.35f * g : g;
    }
    inHp_ = designRbj(FilterType::HighPass, clampv(s_.lowcutHz, 10.0f, 2000.0f), 0.0, 0.7071, fs_);
    inLp_ = designRbj(FilterType::LowPass, std::min<double>(clampv(s_.highcutHz, 500.0f, 22000.0f), 0.45 * fs_), 0.0,
                      0.7071, fs_);
    modDepth_ = (0.15 + 0.25 * s_.size) * 1e-3 * fs_;
    wet_.setTarget(dbToGainf(s_.wetDb), static_cast<int>(0.03 * fs_));
    dry_.setTarget(dbToGainf(s_.dryDb), static_cast<int>(0.03 * fs_));
    updateDecay();
}

void Reverb::updateDecay() {
    const double t60 = s_.decayS;
    const double t60hf = std::max(0.05, t60 * (1.0 - 0.85 * s_.damping));
    for (auto& l : lines_) {
        const double m = l.target;
        const double g = std::pow(10.0, -3.0 * m / (fs_ * t60));
        const double gn = std::pow(10.0, -3.0 * m / (fs_ * t60hf));
        const double r = gn / g;
        l.g = g;
        l.b = (1.0 - r) / (1.0 + r);
    }
}

void Reverb::process(float* left, float* right, int n) {
    const double inv = 2.0 / kLines;
    const double outScale = 0.35;
    const double width = s_.width;
    const double early = s_.early;
    for (int i = 0; i < n; ++i) {
        const float l = left[i], r = right[i];
        const double in = 0.5 * (l + r);
        // Pre-delay buffer holds the band-limited input.
        double x = biquadTick(inLp_, inLpS_, biquadTick(inHp_, inHpS_, in));
        pre_[writePos_ & preMask_] = static_cast<float>(x);
        const int pd = static_cast<int>(preDelay_);
        x = pre_[(writePos_ - pd) & preMask_];

        // Early reflections.
        double erL = 0.0, erR = 0.0;
        if (early > 0.0) {
            for (int k = 0; k < kTaps; ++k) {
                const float v = pre_[(writePos_ - pd - tapDelay_[k]) & preMask_];
                erL += tapGainL_[k] * v;
                erR += tapGainR_[k] * v;
            }
        }

        // Input diffusion.
        double d = x;
        for (auto& ap : diff_) {
            const double v = ap.buf[ap.pos];
            const double y = -ap.g * d + v;
            ap.buf[ap.pos] = static_cast<float>(d + ap.g * y);
            if (++ap.pos == ap.len) ap.pos = 0;
            d = y;
        }

        // Feedback delay network.
        double y[kLines];
        double sum = 0.0;
        for (int k = 0; k < kLines; ++k) {
            Line& ln = lines_[k];
            ln.length += (ln.target - ln.length) * 0.0005;
            double len = ln.length;
            if (k == 1) len += modDepth_ * std::sin(kTwoPi * lfoPhase_[0]);
            if (k == 5) len += modDepth_ * std::sin(kTwoPi * lfoPhase_[1]);
            const double pos = writePos_ - len;
            const double fl = std::floor(pos);
            const int i0 = static_cast<int>(fl);
            const double fr = pos - fl;
            const double v = ln.buf[i0 & ln.mask] * (1.0 - fr) + ln.buf[(i0 + 1) & ln.mask] * fr;
            ln.lp = ln.g * (1.0 - ln.b) * v + ln.b * ln.lp;
            y[k] = ln.lp;
            sum += y[k];
        }
        double wl = 0.0, wr = 0.0;
        for (int k = 0; k < kLines; ++k) {
            const double f = y[k] - inv * sum + kIn[k] * 0.3 * d;
            lines_[k].buf[writePos_ & lines_[k].mask] = static_cast<float>(f);
            wl += kOutL[k] * y[k];
            wr += kOutR[k] * y[k];
        }
        wl = wl * outScale + early * erL;
        wr = wr * outScale + early * erR;
        const double m = 0.5 * (wl + wr), s = 0.5 * (wl - wr) * width;
        const float gw = wet_.next(), gd = dry_.next();
        left[i] = static_cast<float>(gd * l + gw * (m + s));
        right[i] = static_cast<float>(gd * r + gw * (m - s));
        ++writePos_;
        for (int k = 0; k < 2; ++k) {
            lfoPhase_[k] += lfoInc_[k];
            if (lfoPhase_[k] >= 1.0) lfoPhase_[k] -= 1.0;
        }
    }
    // Keep the shared write index bounded (all buffers are powers of two).
    writePos_ &= 0x3FFFFFFF;
}

}  // namespace rfx
