// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/dsp/Biquad.h"

#include <cmath>

#include "rfx/dsp/Common.h"

namespace rfx {

std::complex<double> BiquadCoeffs::response(double w) const {
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
}

double BiquadCoeffs::magnitudeDb(double freqHz, double sampleRate) const {
    return gainToDb(std::abs(response(kTwoPi * freqHz / sampleRate)), -300.0);
}

bool BiquadCoeffs::isStable() const {
    // Stability triangle for 1 + a1 z^-1 + a2 z^-2.
    return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
}

namespace {

struct Normalised {
    double f0, q, A, gainDb;
};

Normalised normalise(double f0, double gainDb, double q, double fs) {
    Normalised n;
    n.f0 = clampv(f0, 1.0, 0.4999 * fs);
    n.q = clampv(q, 0.025, 100.0);
    n.gainDb = clampv(gainDb, -60.0, 60.0);
    n.A = std::pow(10.0, n.gainDb / 40.0);
    return n;
}

BiquadCoeffs fromRaw(double b0, double b1, double b2, double a0, double a1, double a2) {
    BiquadCoeffs c;
    const double inv = 1.0 / a0;
    c.b0 = b0 * inv;
    c.b1 = b1 * inv;
    c.b2 = b2 * inv;
    c.a1 = a1 * inv;
    c.a2 = a2 * inv;
    return c;
}

BiquadCoeffs firstOrder(FilterType type, const Normalised& n, double fs) {
    const double K = std::tan(kPi * n.f0 / fs);
    const double G = std::pow(10.0, n.gainDb / 20.0);
    const double sg = std::sqrt(G);
    switch (type) {
        case FilterType::LowPass1:
            return fromRaw(K, K, 0.0, 1.0 + K, K - 1.0, 0.0);
        case FilterType::HighPass1:
            return fromRaw(1.0, -1.0, 0.0, 1.0 + K, K - 1.0, 0.0);
        case FilterType::LowShelf1:
            // H(s) = (s + sqrt(G)) / (s + 1/sqrt(G)), s normalised to w0 (gain/2 at f0).
            return fromRaw(1.0 + K * sg, K * sg - 1.0, 0.0, 1.0 + K / sg, K / sg - 1.0, 0.0);
        case FilterType::HighShelf1:
            // H(s) = G (s + 1/sqrt(G)) / (s + sqrt(G)).
            return fromRaw(G * (1.0 + K / sg), G * (K / sg - 1.0), 0.0, 1.0 + K * sg, K * sg - 1.0, 0.0);
        case FilterType::Tilt: {
            BiquadCoeffs c = fromRaw(G * (1.0 + K / sg), G * (K / sg - 1.0), 0.0, 1.0 + K * sg,
                                     K * sg - 1.0, 0.0);
            const double k = 1.0 / sg;  // -gain/2 overall so the pivot sits at 0 dB
            c.b0 *= k;
            c.b1 *= k;
            return c;
        }
        default:
            return {};
    }
}

bool isFirstOrder(FilterType t) {
    return t == FilterType::LowPass1 || t == FilterType::HighPass1 || t == FilterType::LowShelf1 ||
           t == FilterType::HighShelf1 || t == FilterType::Tilt;
}

// Analog prototype as polynomials in s normalised to w0: N(s) = n2 s^2 + n1 s + n0,
// D(s) = d2 s^2 + d1 s + d0.
struct Analog {
    double n2, n1, n0, d2, d1, d0;
    double mag2(double W) const {
        const double nr = n0 - n2 * W * W, ni = n1 * W;
        const double dr = d0 - d2 * W * W, di = d1 * W;
        return (nr * nr + ni * ni) / (dr * dr + di * di);
    }
};

bool analogPrototype(FilterType type, const Normalised& n, Analog& a) {
    const double A = n.A, Q = n.q, sA = std::sqrt(A);
    switch (type) {
        case FilterType::Peak: a = {1.0, A / Q, 1.0, 1.0, 1.0 / (A * Q), 1.0}; return true;
        case FilterType::LowShelf: a = {A, A * sA / Q, A * A, A, sA / Q, 1.0}; return true;
        case FilterType::HighShelf: a = {A * A, A * sA / Q, A, 1.0, sA / Q, A}; return true;
        case FilterType::LowPass: a = {0.0, 0.0, 1.0, 1.0, 1.0 / Q, 1.0}; return true;
        case FilterType::HighPass: a = {1.0, 0.0, 0.0, 1.0, 1.0 / Q, 1.0}; return true;
        case FilterType::BandPass: a = {0.0, 1.0 / Q, 0.0, 1.0, 1.0 / Q, 1.0}; return true;
        case FilterType::Notch: a = {1.0, 0.0, 1.0, 1.0, 1.0 / Q, 1.0}; return true;
        case FilterType::AllPass: a = {1.0, -1.0 / Q, 1.0, 1.0, 1.0 / Q, 1.0}; return true;
        default: return false;
    }
}

}  // namespace

BiquadCoeffs designRbj(FilterType type, double f0, double gainDb, double q, double fs) {
    const Normalised n = normalise(f0, gainDb, q, fs);
    if (isFirstOrder(type)) return firstOrder(type, n, fs);

    const double w0 = kTwoPi * n.f0 / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * n.q);
    const double A = n.A, sA = std::sqrt(A);

    switch (type) {
        case FilterType::Peak:
            return fromRaw(1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * cw,
                           1.0 - alpha / A);
        case FilterType::LowShelf:
            return fromRaw(A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * sA * alpha),
                           2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                           A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sA * alpha),
                           (A + 1.0) + (A - 1.0) * cw + 2.0 * sA * alpha,
                           -2.0 * ((A - 1.0) + (A + 1.0) * cw),
                           (A + 1.0) + (A - 1.0) * cw - 2.0 * sA * alpha);
        case FilterType::HighShelf:
            return fromRaw(A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * sA * alpha),
                           -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                           A * ((A + 1.0) + (A - 1.0) * cw - 2.0 * sA * alpha),
                           (A + 1.0) - (A - 1.0) * cw + 2.0 * sA * alpha,
                           2.0 * ((A - 1.0) - (A + 1.0) * cw),
                           (A + 1.0) - (A - 1.0) * cw - 2.0 * sA * alpha);
        case FilterType::LowPass:
            return fromRaw((1.0 - cw) / 2.0, 1.0 - cw, (1.0 - cw) / 2.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
        case FilterType::HighPass:
            return fromRaw((1.0 + cw) / 2.0, -(1.0 + cw), (1.0 + cw) / 2.0, 1.0 + alpha, -2.0 * cw,
                           1.0 - alpha);
        case FilterType::BandPass:
            return fromRaw(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
        case FilterType::Notch:
            return fromRaw(1.0, -2.0 * cw, 1.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
        case FilterType::AllPass:
            return fromRaw(1.0 - alpha, -2.0 * cw, 1.0 + alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
        default:
            return {};
    }
}

namespace {
BiquadCoeffs designMatchedDirect(FilterType type, double f0, double gainDb, double q, double fs);

// Inverse of a biquad (swap numerator and denominator), or nullopt-like failure via `ok`.
BiquadCoeffs invert(const BiquadCoeffs& c, bool& ok) {
    BiquadCoeffs r;
    ok = std::fabs(c.b0) > 1e-12;
    if (!ok) return r;
    r.b0 = 1.0 / c.b0;
    r.b1 = c.a1 / c.b0;
    r.b2 = c.a2 / c.b0;
    r.a1 = c.b1 / c.b0;
    r.a2 = c.b2 / c.b0;
    ok = r.isStable();
    return r;
}
}  // namespace

BiquadCoeffs designMatched(FilterType type, double f0, double gainDb, double q, double fs) {
    // Peak, low- and high-shelf prototypes satisfy H(-g) = 1/H(g).  Impulse invariance only
    // works when the poles sit well below Nyquist, so design the direction whose poles are
    // lowest (cuts for high shelves, boosts for low shelves and peaks) and invert if needed.
    bool flip = false;
    if (type == FilterType::LowShelf || type == FilterType::Peak) flip = gainDb < 0.0;
    if (type == FilterType::HighShelf) flip = gainDb > 0.0;
    if (!flip) return designMatchedDirect(type, f0, gainDb, q, fs);
    bool ok = false;
    BiquadCoeffs inv = invert(designMatchedDirect(type, f0, -gainDb, q, fs), ok);
    return ok ? inv : designRbj(type, f0, gainDb, q, fs);
}

namespace {
BiquadCoeffs designMatchedDirect(FilterType type, double f0, double gainDb, double q, double fs) {
    const Normalised n = normalise(f0, gainDb, q, fs);
    Analog a;
    const double w0 = kTwoPi * n.f0 / fs;
    // Below ~0.8 % of fs the bilinear design is already indistinguishable from the analog
    // prototype, and the matched solution becomes numerically delicate.  All-pass and notch
    // filters have exact bilinear equivalents in the sense that matters (phase / null).
    if (isFirstOrder(type) || w0 < 0.05 || type == FilterType::AllPass || type == FilterType::Notch ||
        !analogPrototype(type, n, a)) {
        return designRbj(type, f0, gainDb, q, fs);
    }

    // Poles by impulse invariance.
    double a1, a2;
    const double disc = a.d1 * a.d1 - 4.0 * a.d2 * a.d0;
    if (disc < 0.0) {
        const double sigma = -a.d1 / (2.0 * a.d2) * w0;
        const double omega = std::sqrt(-disc) / (2.0 * a.d2) * w0;
        a1 = -2.0 * std::exp(sigma) * std::cos(omega);
        a2 = std::exp(2.0 * sigma);
    } else {
        const double r = std::sqrt(disc);
        const double p1 = (-a.d1 + r) / (2.0 * a.d2) * w0;
        const double p2 = (-a.d1 - r) / (2.0 * a.d2) * w0;
        a1 = -(std::exp(p1) + std::exp(p2));
        a2 = std::exp(p1 + p2);
    }

    // |A(phi)|^2 = A0 (1-phi) + A1 phi + A2 4 phi (1-phi),  phi = sin^2(w/2).
    const double A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2);
    const double A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2);
    const double A2 = -4.0 * a2;

    const double tDc = a.mag2(0.0);
    const double tNyq = a.mag2(kPi / w0);
    const double tMid = a.mag2(1.0);

    const double phi1 = std::sin(w0 / 2.0) * std::sin(w0 / 2.0);
    const double phi0 = 1.0 - phi1;
    const double phi2 = 4.0 * phi0 * phi1;

    const double B0 = tDc * A0;
    const double B1 = tNyq * A1;
    const double B2 = (tMid * (A0 * phi0 + A1 * phi1 + A2 * phi2) - B0 * phi0 - B1 * phi1) / phi2;

    const double sB0 = std::sqrt(std::max(0.0, B0));
    const double sB1 = std::sqrt(std::max(0.0, B1));
    const double W = 0.5 * (sB0 + sB1);
    const double b0 = 0.5 * (W + std::sqrt(std::max(0.0, W * W + B2)));
    const double b1 = 0.5 * (sB0 - sB1);
    const double b2 = W - b0;

    BiquadCoeffs c;
    c.b0 = b0;
    c.b1 = b1;
    c.b2 = b2;
    c.a1 = a1;
    c.a2 = a2;
    return c;
}
}  // namespace

double analogMagnitudeDb(FilterType type, double f0, double gainDb, double q, double freqHz) {
    Normalised n;
    n.f0 = std::max(1.0, f0);
    n.q = clampv(q, 0.025, 100.0);
    n.gainDb = clampv(gainDb, -60.0, 60.0);
    n.A = std::pow(10.0, n.gainDb / 40.0);
    const double W = freqHz / n.f0;
    if (isFirstOrder(type)) {
        const double G = std::pow(10.0, n.gainDb / 20.0), sg = std::sqrt(G);
        double m2 = 1.0;
        switch (type) {
            case FilterType::LowPass1: m2 = 1.0 / (1.0 + W * W); break;
            case FilterType::HighPass1: m2 = W * W / (1.0 + W * W); break;
            case FilterType::LowShelf1: m2 = (W * W + G) / (W * W + 1.0 / G); break;
            case FilterType::HighShelf1: m2 = G * G * (W * W + 1.0 / G) / (W * W + G); break;
            case FilterType::Tilt: m2 = G * (W * W + 1.0 / G) / (W * W + G); break;
            default: break;
        }
        (void)sg;
        return powerToDb(m2, -300.0);
    }
    Analog a;
    if (!analogPrototype(type, n, a)) return 0.0;
    return powerToDb(a.mag2(W), -300.0);
}

int butterworthQs(int order, double* qs, bool* hasFirstOrder) {
    order = clampv(order, 1, 16);
    const int sections = order / 2;
    for (int k = 1; k <= sections; ++k) {
        const double theta = (2.0 * k - 1.0) * kPi / (2.0 * order);
        qs[k - 1] = 1.0 / (2.0 * std::cos(theta));
    }
    if (hasFirstOrder) *hasFirstOrder = (order % 2) == 1;
    return sections;
}

}  // namespace rfx
