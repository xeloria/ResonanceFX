// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Radix-2 FFT with a real-input wrapper (N-point real transform via an N/2-point
// complex transform).  Header-only; instantiated for float (convolution) and
// double (filter design).

#include <cmath>
#include <vector>

namespace rfx {

template <typename T>
class Fft {
public:
    Fft() = default;
    explicit Fft(int n) { init(n); }

    /// n: real transform size, power of two >= 4.  Allocates; not real-time safe.
    void init(int n) {
        n_ = n;
        m_ = n / 2;
        log2m_ = 0;
        while ((1 << log2m_) < m_) ++log2m_;
        bitrev_.resize(m_);
        for (int i = 0; i < m_; ++i) {
            int r = 0;
            for (int b = 0; b < log2m_; ++b)
                if (i & (1 << b)) r |= 1 << (log2m_ - 1 - b);
            bitrev_[i] = r;
        }
        cos_.resize(m_ / 2 + 1);
        sin_.resize(m_ / 2 + 1);
        for (int k = 0; k <= m_ / 2; ++k) {
            const double a = 2.0 * 3.14159265358979323846 * k / m_;
            cos_[k] = static_cast<T>(std::cos(a));
            sin_[k] = static_cast<T>(std::sin(a));
        }
        wr_.resize(m_ + 1);
        wi_.resize(m_ + 1);
        for (int k = 0; k <= m_; ++k) {
            const double a = -2.0 * 3.14159265358979323846 * k / n_;
            wr_[k] = static_cast<T>(std::cos(a));
            wi_[k] = static_cast<T>(std::sin(a));
        }
        zr_.assign(m_, T(0));
        zi_.assign(m_, T(0));
    }

    int size() const { return n_; }
    int bins() const { return m_ + 1; }

    /// Forward real FFT (unscaled): in[n] -> re[n/2+1], im[n/2+1].
    void forward(const T* in, T* re, T* im) {
        for (int k = 0; k < m_; ++k) {
            zr_[k] = in[2 * k];
            zi_[k] = in[2 * k + 1];
        }
        complexInPlace(zr_.data(), zi_.data(), false);
        // Split the packed spectrum: X[k] = Fe[k] + W^k Fo[k].
        re[0] = zr_[0] + zi_[0];
        im[0] = 0;
        re[m_] = zr_[0] - zi_[0];
        im[m_] = 0;
        for (int k = 1; k < m_; ++k) {
            const T ar = zr_[k], ai = zi_[k];
            const T br = zr_[m_ - k], bi = -zi_[m_ - k];  // conj(Z[M-k])
            const T fer = (ar + br) * T(0.5), fei = (ai + bi) * T(0.5);
            // Fo = (Z[k] - conj(Z[M-k])) / (2i)
            const T dr = (ar - br) * T(0.5), di = (ai - bi) * T(0.5);
            const T for_ = di, foi = -dr;
            re[k] = fer + wr_[k] * for_ - wi_[k] * foi;
            im[k] = fei + wr_[k] * foi + wi_[k] * for_;
        }
    }

    /// Inverse real FFT, scaled so that inverse(forward(x)) == x.
    void inverse(const T* re, const T* im, T* out) {
        for (int k = 0; k < m_; ++k) {
            const T ar = re[k], ai = im[k];
            const T br = re[m_ - k], bi = -im[m_ - k];  // conj(X[M-k])
            const T fer = (ar + br) * T(0.5), fei = (ai + bi) * T(0.5);
            const T dr = (ar - br) * T(0.5), di = (ai - bi) * T(0.5);
            // Fo = (X[k] - conj(X[M-k])) / 2 * W^-k
            const T for_ = dr * wr_[k] + di * wi_[k];
            const T foi = di * wr_[k] - dr * wi_[k];
            // Z = Fe + i Fo
            zr_[k] = fer - foi;
            zi_[k] = fei + for_;
        }
        complexInPlace(zr_.data(), zi_.data(), true);
        const T scale = T(1) / static_cast<T>(m_);
        for (int k = 0; k < m_; ++k) {
            out[2 * k] = zr_[k] * scale;
            out[2 * k + 1] = zi_[k] * scale;
        }
    }

private:
    void complexInPlace(T* re, T* im, bool inv) {
        const int m = m_;
        for (int i = 0; i < m; ++i) {
            const int j = bitrev_[i];
            if (j > i) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        const T sign = inv ? T(1) : T(-1);
        for (int len = 2; len <= m; len <<= 1) {
            const int half = len >> 1;
            const int step = m / len;
            for (int i = 0; i < m; i += len) {
                for (int j = 0; j < half; ++j) {
                    const T wr = cos_[j * step];
                    const T wi = sign * sin_[j * step];
                    T* ar = re + i + j;
                    T* ai = im + i + j;
                    T* br = re + i + j + half;
                    T* bi = im + i + j + half;
                    const T tr = *br * wr - *bi * wi;
                    const T ti = *br * wi + *bi * wr;
                    *br = *ar - tr;
                    *bi = *ai - ti;
                    *ar += tr;
                    *ai += ti;
                }
            }
        }
    }

    int n_ = 0, m_ = 0, log2m_ = 0;
    std::vector<int> bitrev_;
    std::vector<T> cos_, sin_, wr_, wi_, zr_, zi_;
};

}  // namespace rfx
