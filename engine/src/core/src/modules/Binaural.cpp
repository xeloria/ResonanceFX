// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/modules/Binaural.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <mutex>

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

/// Head orientation as a unit quaternion (w, x, y, z), head -> world. A tracker-free renderer
/// is the identity; the legacy path uses yaw only (positive = head turned right).
void orientationQuat(const HrtfSpatializer::Settings& s, float* q) {
    if (!s.tracking) {
        q[0] = 1.0f; q[1] = q[2] = q[3] = 0.0f;
        return;
    }
    if (!s.orientation) {
        const float h = 0.5f * s.yawDeg * static_cast<float>(kPi / 180.0);
        q[0] = std::cos(h); q[1] = 0.0f; q[2] = 0.0f; q[3] = -std::sin(h);
        return;
    }
    float w = s.quatW, x = s.quatX, y = s.quatY, z = s.quatZ;
    const float n = std::sqrt(w * w + x * x + y * y + z * z);
    if (!(n > 1e-6f) || !std::isfinite(n)) {
        q[0] = 1.0f; q[1] = q[2] = q[3] = 0.0f;
        return;
    }
    q[0] = w / n; q[1] = x / n; q[2] = y / n; q[3] = z / n;
}

/// R^T (world -> head) for a unit quaternion, row-major.
void worldToHead(const float* q, float* m) {
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    // R (head -> world), then transposed.
    m[0] = 1.0f - 2.0f * (y * y + z * z);
    m[1] = 2.0f * (x * y + w * z);
    m[2] = 2.0f * (x * z - w * y);
    m[3] = 2.0f * (x * y - w * z);
    m[4] = 1.0f - 2.0f * (x * x + z * z);
    m[5] = 2.0f * (y * z + w * x);
    m[6] = 2.0f * (x * z + w * y);
    m[7] = 2.0f * (y * z - w * x);
    m[8] = 1.0f - 2.0f * (x * x + y * y);
}

struct ResampleTap {
    int index;
    float weight;
};
}  // namespace

std::shared_ptr<const HrtfSpatializer::Table> HrtfSpatializer::sharedTable(double fs) {
    // One table per rate, shared by every renderer in the process. The cache keeps the last
    // few rates so switching output devices (44.1/48/96 kHz) does not rebuild every time.
    static std::mutex mutex;
    static std::vector<std::pair<double, std::shared_ptr<const Table>>> cache;
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& entry : cache) {
        if (std::fabs(entry.first - fs) < 1e-6) return entry.second;
    }
    auto built = std::make_shared<Table>();
    buildTable(fs, *built);
    cache.emplace_back(fs, built);
    if (cache.size() > 3) cache.erase(cache.begin());
    return built;
}

void HrtfSpatializer::buildTable(double fs, Table& table) {
    table.taps = fs <= 50000.0 ? 128 : (fs <= 100000.0 ? 256 : 512);
    const int taps = table.taps;
    const int M = 2048;
    const int bins = M / 2 + 1;
    Fft<double> fft(M);
    std::vector<double> buf(M), re(bins), im(bins);
    const int directions = kElGrid * kAzGrid;
    std::vector<std::vector<double>> mags(static_cast<size_t>(directions) * 2,
                                          std::vector<double>(bins));
    std::vector<double> avg(bins, 0.0);
    std::vector<float> source(kemar::kTaps);

    // The resampling weights depend only on the two rates, so build them once instead of
    // running the Kaiser-windowed sinc (and besselI0) per direction.
    std::vector<std::vector<ResampleTap>> plan;
    int resampledFrames = kemar::kTaps;
    if (std::fabs(fs - kemar::kSampleRate) > 1e-6) {
        const double ratio = fs / kemar::kSampleRate;
        resampledFrames = static_cast<int>(std::ceil(kemar::kTaps * ratio));
        const double fc = std::min(1.0, ratio) * 0.95;
        const double halfWidth = 32.0 / fc;
        const double beta = 9.0;
        plan.resize(static_cast<size_t>(resampledFrames));
        for (int j = 0; j < resampledFrames; ++j) {
            const double t = j / ratio;
            const int i0 = std::max(0, static_cast<int>(std::ceil(t - halfWidth)));
            const int i1 = std::min(kemar::kTaps - 1, static_cast<int>(std::floor(t + halfWidth)));
            plan[static_cast<size_t>(j)].reserve(static_cast<size_t>(std::max(0, i1 - i0 + 1)));
            for (int i = i0; i <= i1; ++i) {
                const double x = t - i;
                const double arg = kPi * fc * x;
                const double s = std::fabs(arg) < 1e-12 ? 1.0 : std::sin(arg) / arg;
                const float w = static_cast<float>(fc * s * kaiserWindow(x / halfWidth, beta));
                plan[static_cast<size_t>(j)].push_back({i, w});
            }
        }
    }

    // Resample, measure onset delay and magnitude of every measured response.
    for (int e = 0; e < kElGrid; ++e) {
        for (int a = 0; a < kAzGrid; ++a) {
            for (int ear = 0; ear < 2; ++ear) {
                for (int k = 0; k < kemar::kTaps; ++k)
                    source[k] = static_cast<float>(kemar::kHrir[e][a][ear][k]) * (1.0f / 32768.0f);
                std::vector<float> h(static_cast<size_t>(resampledFrames), 0.0f);
                if (plan.empty()) {
                    std::copy(source.begin(), source.end(), h.begin());
                } else {
                    for (int j = 0; j < resampledFrames; ++j) {
                        float acc = 0.0f;
                        for (const ResampleTap& tap : plan[static_cast<size_t>(j)])
                            acc += source[static_cast<size_t>(tap.index)] * tap.weight;
                        h[static_cast<size_t>(j)] = acc;
                    }
                }
                float peak = 0.0f;
                for (float v : h) peak = std::max(peak, std::fabs(v));
                int onset = 0;
                while (onset < resampledFrames && std::fabs(h[static_cast<size_t>(onset)]) < 0.2f * peak)
                    ++onset;
                table.delay[e][a][ear] = std::max(0, onset - 1);
                std::fill(buf.begin(), buf.end(), 0.0);
                for (int k = 0; k < resampledFrames && k < M; ++k) buf[k] = h[static_cast<size_t>(k)];
                fft.forward(buf.data(), re.data(), im.data());
                auto& mg = mags[(static_cast<size_t>(e) * kAzGrid + a) * 2 + ear];
                for (int k = 0; k < bins; ++k) {
                    mg[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
                    avg[k] += mg[k] * mg[k];
                }
            }
        }
    }
    // Diffuse-field equalisation: invert the (1/3-octave smoothed) average power response.
    for (auto& v : avg) v /= directions * 2.0;
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
    table.hrir.assign(static_cast<size_t>(directions) * 2 * taps, 0.0f);
    std::vector<double> mag(bins);
    for (int e = 0; e < kElGrid; ++e) {
        for (int a = 0; a < kAzGrid; ++a) {
            for (int ear = 0; ear < 2; ++ear) {
                const auto& mg = mags[(static_cast<size_t>(e) * kAzGrid + a) * 2 + ear];
                for (int k = 0; k < bins; ++k) mag[k] = mg[k] * eq[k];
                std::vector<float> h = designFirFromMagnitude(mag, taps, true);
                float* dst = table.hrir.data() + ((static_cast<size_t>(e) * kAzGrid + a) * 2 + ear) * taps;
                for (int k = 0; k < taps; ++k) dst[k] = h[taps - 1 - k];
            }
        }
    }

    // Normalise so a centred (mono) signal through the default +/-30 degree pair is 0 dB at 1 kHz.
    {
        Filter f30, fm30;
        interpolateInto(table, 30.0f, 0.0f, f30);
        interpolateInto(table, -30.0f, 0.0f, fm30);
        const double w = kTwoPi * 1000.0 / fs;
        std::complex<double> sum = 0.0;
        for (const Filter* f : {&f30, &fm30}) {
            for (int k = 0; k < taps; ++k)
                sum += static_cast<double>(f->h[0][taps - 1 - k]) * std::polar(1.0, -w * (k + f->delay[0]));
        }
        table.norm = static_cast<float>(clampv(1.0 / std::max(std::abs(sum), 1e-6), 0.1, 2.0));
    }

    const int maxDelay = static_cast<int>(0.012 * fs) + 2 * taps + 64;
    table.histMask = nextPow2(maxDelay) - 1;
}

void HrtfSpatializer::prepare(double fs) {
    fs_ = fs;
    table_ = sharedTable(fs);
    taps_ = table_->taps;
    histMask_ = table_->histMask;
    const int size = histMask_ + 1;
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
    for (int s = 0; s < kSources; ++s) cur_[s].az = cur_[s].el = 1e9f;
}

void HrtfSpatializer::headDirection(float azimuthDeg, float elevationDeg, float qw, float qx, float qy,
                                    float qz, float* outAzimuthDeg, float* outElevationDeg) {
    const float n = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
    const float inv = (n > 1e-6f && std::isfinite(n)) ? 1.0f / n : 0.0f;
    const float q[4] = {inv > 0.0f ? qw * inv : 1.0f, qx * inv, qy * inv, qz * inv};
    float m[9];
    worldToHead(q, m);
    const float az = azimuthDeg * static_cast<float>(kPi / 180.0);
    const float el = elevationDeg * static_cast<float>(kPi / 180.0);
    const float cx = std::sin(az) * std::cos(el);
    const float cy = std::cos(az) * std::cos(el);
    const float cz = std::sin(el);
    const float hx = m[0] * cx + m[1] * cy + m[2] * cz;
    const float hy = m[3] * cx + m[4] * cy + m[5] * cz;
    const float hz = m[6] * cx + m[7] * cy + m[8] * cz;
    if (outAzimuthDeg) *outAzimuthDeg = std::atan2(hx, hy) * static_cast<float>(180.0 / kPi);
    if (outElevationDeg)
        *outElevationDeg = std::asin(clampv(hz, -1.0f, 1.0f)) * static_cast<float>(180.0 / kPi);
}

void HrtfSpatializer::interpolateInto(const Table& table, float az, float elevationDeg, Filter& out) {
    const int taps = table.taps;
    while (az > 180.0f) az -= 360.0f;
    while (az <= -180.0f) az += 360.0f;
    const bool mirror = az < 0.0f;
    const float a = std::fabs(az) / 5.0f;
    const int a0 = std::min(static_cast<int>(a), kAzGrid - 1);
    const int a1 = std::min(a0 + 1, kAzGrid - 1);
    const float ta = a - a0;

    const float eg = clampv((elevationDeg - kemar::kElevationMin) / static_cast<float>(kemar::kElevationStep),
                            0.0f, static_cast<float>(kElGrid - 1));
    const int e0 = std::min(static_cast<int>(eg), kElGrid - 1);
    const int e1 = std::min(e0 + 1, kElGrid - 1);
    const float te = eg - e0;

    for (int ear = 0; ear < 2; ++ear) {
        const int src = mirror ? 1 - ear : ear;
        const float* t00 = table.hrir.data() + ((static_cast<size_t>(e0) * kAzGrid + a0) * 2 + src) * taps;
        const float* t01 = table.hrir.data() + ((static_cast<size_t>(e0) * kAzGrid + a1) * 2 + src) * taps;
        const float* t10 = table.hrir.data() + ((static_cast<size_t>(e1) * kAzGrid + a0) * 2 + src) * taps;
        const float* t11 = table.hrir.data() + ((static_cast<size_t>(e1) * kAzGrid + a1) * 2 + src) * taps;
        out.h[ear].resize(taps);
        for (int k = 0; k < taps; ++k) {
            const float h0 = t00[k] + ta * (t01[k] - t00[k]);
            const float h1 = t10[k] + ta * (t11[k] - t10[k]);
            out.h[ear][k] = h0 + te * (h1 - h0);
        }
        const float d0 = static_cast<float>(table.delay[e0][a0][src]) +
                         ta * (table.delay[e0][a1][src] - table.delay[e0][a0][src]);
        const float d1 = static_cast<float>(table.delay[e1][a0][src]) +
                         ta * (table.delay[e1][a1][src] - table.delay[e1][a0][src]);
        out.delay[ear] = static_cast<int>(std::lround(d0 + te * (d1 - d0)));
    }
    out.az = az;
    out.el = elevationDeg;
}

void HrtfSpatializer::interpolate(float az, float elevationDeg, Filter& out) const {
    interpolateInto(*table_, az, elevationDeg, out);
}

void HrtfSpatializer::response(float az, float elevationDeg, std::vector<float>& l, std::vector<float>& r,
                               float* dl, float* dr) const {
    Filter f;
    interpolate(az, elevationDeg, f);
    l.assign(f.h[0].rbegin(), f.h[0].rend());
    r.assign(f.h[1].rbegin(), f.h[1].rend());
    if (dl) *dl = static_cast<float>(f.delay[0]);
    if (dr) *dr = static_cast<float>(f.delay[1]);
}

void HrtfSpatializer::impulseResponse(const Settings& settings, std::vector<float>& left,
                                      std::vector<float>& right, int n) const {
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    float q[4];
    orientationQuat(settings, q);
    const float angle = clampv(settings.angleDeg, 5.0f, 90.0f);
    const int sources = settings.room > 0.001f ? 4 : 2;
    const float refl = 0.35f * clampv(settings.room, 0.0f, 1.0f);
    for (int s = 0; s < sources; ++s) {
        const float base = s < 2 ? kBaseAz[s] * angle : kBaseAz[s];
        float az = 0.0f, el = 0.0f;
        headDirection(base, 0.0f, q[0], q[1], q[2], q[3], &az, &el);
        Filter f;
        interpolate(az, el, f);
        const int extra = s == 2 ? static_cast<int>(0.007 * fs_) : (s == 3 ? static_cast<int>(0.011 * fs_) : 0);
        const float g = (s < 2 ? 1.0f : refl) * table_->norm;
        for (int ear = 0; ear < 2; ++ear) {
            const int off = extra + f.delay[ear];
            std::vector<float>& out = ear == 0 ? left : right;
            for (int k = 0; k < taps_; ++k) {
                const int at = off + k;
                if (at >= n) break;
                out[at] += g * f.h[ear][taps_ - 1 - k];
            }
        }
    }
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
    float q[4];
    orientationQuat(s_, q);
    const float angle = clampv(s_.angleDeg, 5.0f, 90.0f);
    const int sources = s_.room > 0.001f ? 4 : 2;
    const float refl = 0.35f * clampv(s_.room, 0.0f, 1.0f);
    const int size = histMask_ + 1;
    for (int off = 0; off < n; off += kBlock) {
        const int m = std::min(kBlock, n - off);
        bool change[kSources] = {false, false, false, false};
        for (int s = 0; s < sources; ++s) {
            const float base = s < 2 ? kBaseAz[s] * angle : kBaseAz[s];
            float az = 0.0f, el = 0.0f;
            headDirection(base, 0.0f, q[0], q[1], q[2], q[3], &az, &el);
            if (std::fabs(az - cur_[s].az) > 0.2f || std::fabs(el - cur_[s].el) > 0.2f) {
                interpolate(az, el, next_[s]);
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
            left[off + i] = static_cast<float>(out[0] * table_->norm);
            right[off + i] = static_cast<float>(out[1] * table_->norm);
            ++t_;
        }
        for (int s = 0; s < sources; ++s)
            if (change[s]) cur_[s] = next_[s];
    }
}

}  // namespace rfx
