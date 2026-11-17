// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#include "Fx.h"

#include <algorithm>
#include <cmath>

#include "rfx/dsp/Common.h"

namespace rfx::fx {

// ---- BiquadCascade ----------------------------------------------------------------------------

void BiquadCascade::prepare(double sampleRate) {
    fs_ = sampleRate;
    rebuild();
}

void BiquadCascade::reset() {
    for (auto& s : state_) s.fill(0.0);
}

bool BiquadCascade::setCoefficients(uint32_t count, const float* at44k1, const float* at48k) {
    if (count == 0 || count % 5 != 0 || count / 5 > kMaxSections) return false;
    auto load = [count](const float* c, std::vector<Section>& out) {
        out.clear();
        for (uint32_t i = 0; i < count; i += 5) {
            Section s{c[i], c[i + 1], c[i + 2], -static_cast<double>(c[i + 3]), -static_cast<double>(c[i + 4])};
            if (!std::isfinite(s.b0) || !std::isfinite(s.b1) || !std::isfinite(s.b2) || !std::isfinite(s.a1) ||
                !std::isfinite(s.a2)) {
                return false;
            }
            out.push_back(s);
        }
        return true;
    };
    std::vector<Section> a, b;
    if (!load(at44k1, a) || !load(at48k, b)) return false;
    src44_ = std::move(a);
    src48_ = std::move(b);
    rebuild();
    return true;
}

void BiquadCascade::rebuild() {
    if (std::fabs(fs_ - 44100.0) < 1.0) {
        active_ = src44_;
    } else if (std::fabs(fs_ - 48000.0) < 1.0 || src48_.empty()) {
        active_ = src48_;
    } else {
        // Re-target each 48 kHz section: z-domain -> analog prototype (inverse bilinear at 48 kHz)
        // -> z-domain at the output rate. Exact in shape below a few kHz; near Nyquist the two
        // warpings differ slightly, which matters little for headphone correction curves.
        active_.clear();
        const double k1 = 2.0 * 48000.0, k2 = 2.0 * fs_;
        for (const Section& s : src48_) {
            // Polynomials in z: N = b0 z^2 + b1 z + b2, D = z^2 + a1 z + a2.
            // z = (k1 + s)/(k1 - s): N(s) = n2 s^2 + n1 s + n0.
            auto toAnalog = [k1](double c0, double c1, double c2, double& p2, double& p1, double& p0) {
                p2 = c0 - c1 + c2;
                p1 = 2.0 * k1 * (c0 - c2);
                p0 = k1 * k1 * (c0 + c1 + c2);
            };
            double n2, n1, n0, d2, d1, d0;
            toAnalog(s.b0, s.b1, s.b2, n2, n1, n0);
            toAnalog(1.0, s.a1, s.a2, d2, d1, d0);
            // s = k2 (z - 1)/(z + 1): P(z) = (p2 k2^2 + p1 k2 + p0) z^2 + 2(p0 - p2 k2^2) z
            //                              + (p2 k2^2 - p1 k2 + p0)
            auto toDigital = [k2](double p2, double p1, double p0, double& c0, double& c1, double& c2) {
                c0 = p2 * k2 * k2 + p1 * k2 + p0;
                c1 = 2.0 * (p0 - p2 * k2 * k2);
                c2 = p2 * k2 * k2 - p1 * k2 + p0;
            };
            double b0, b1, b2, a0, a1, a2;
            toDigital(n2, n1, n0, b0, b1, b2);
            toDigital(d2, d1, d0, a0, a1, a2);
            if (std::fabs(a0) < 1e-30) continue;
            active_.push_back({b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0});
        }
    }
    state_.assign(active_.size(), {});
}

void BiquadCascade::process(float* left, float* right, int n) {
    const size_t k = active_.size();
    if (k == 0) return;
    for (int i = 0; i < n; ++i) {
        double l = left[i], r = right[i];
        for (size_t j = 0; j < k; ++j) {
            const Section& c = active_[j];
            auto& s = state_[j];
            const double yl = c.b0 * l + c.b1 * s[0] + c.b2 * s[1] - c.a1 * s[2] - c.a2 * s[3];
            s[1] = s[0]; s[0] = l; s[3] = s[2]; s[2] = yl; l = yl;
            const double yr = c.b0 * r + c.b1 * s[4] + c.b2 * s[5] - c.a1 * s[6] - c.a2 * s[7];
            s[5] = s[4]; s[4] = r; s[7] = s[6]; s[6] = yr; r = yr;
        }
        left[i] = static_cast<float>(l);
        right[i] = static_cast<float>(r);
    }
}

// ---- FieldSurround ----------------------------------------------------------------------------

void FieldSurround::prepare(double sampleRate) {
    fs_ = sampleRate;
    hp_ = designRbj(FilterType::HighPass, 180.0, 0.0, 0.7071, fs_);
    reset();
}

void FieldSurround::reset() {
    hpL_.reset();
    hpR_.reset();
}

void FieldSurround::set(float strength, float midLevel) {
    // Strength 0..1 widens the side signal by up to +8 dB (x2.5); mid 0..1 sets the centre image
    // from -6 dB to +3.5 dB with 0.5 = unity.
    sideGain_ = 1.0f + 1.5f * clampv(strength, 0.0f, 1.0f);
    const float m = clampv(midLevel, 0.0f, 1.0f);
    midGain_ = m <= 0.5f ? 0.5f + m : 1.0f + (m - 0.5f);
}

void FieldSurround::process(float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const double mid = 0.5 * (left[i] + right[i]);
        const double side = 0.5 * (left[i] - right[i]);
        // Only the side signal above ~180 Hz is boosted; bass side content stays as it was.
        const double sideHigh = biquadTick(hp_, hpL_, side);
        const double sideOut = side + (sideGain_ - 1.0) * sideHigh;
        const double midOut = mid * midGain_;
        left[i] = static_cast<float>(midOut + sideOut);
        right[i] = static_cast<float>(midOut - sideOut);
    }
}

// ---- HaasDelay ----------------------------------------------------------------------------------

void HaasDelay::prepare(double sampleRate) {
    fs_ = sampleRate;
    buf_.assign(static_cast<size_t>(std::ceil(0.021 * fs_)) + 1, 0.0f);
    pos_ = 0;
    
    setHighpassHz(hpHz_); 
    
    lfoInc_ = 0.1 / fs_;
    lfoPhase_ = 0.0;
    envCoef_ = 1.0f - std::exp(-64.0f / (0.3f * fs_));
    
    // Apply target delay directly based on the new sample rate to prevent reset state loss
    delay_ = std::min(static_cast<int>(std::lround(targetDelayMs_ * fs_ / 1000.0)), static_cast<int>(buf_.size()) - 1);
    pendingDelayMs_ = -1.0f;
    dipRemaining_ = 0;
    dipTotal_ = 0;
}

void HaasDelay::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    pos_ = 0;
    hpState_.reset();
    lfoPhase_ = 0.0;
    midEnv_ = 0.0f;
    sideEnv_ = 0.0f;
    envCounter_ = 0;
    isMonoActive_ = false;
    monoFactor_ = 1.0f;
    
    // Re-align delay and clear transition states to avoid lingering effects after reset
    delay_ = std::min(static_cast<int>(std::lround(targetDelayMs_ * fs_ / 1000.0)), static_cast<int>(buf_.size()) - 1);
    dipRemaining_ = 0;
    dipTotal_ = 0;
    pendingDelayMs_ = -1.0f;
    
    widthCurrent_ = width_;
    widthRampRemaining_ = 0;
    widthStep_ = 0.0f;
}

void HaasDelay::setDelayMs(float ms) {
    targetDelayMs_ = clampv(ms, 0.0f, 20.0f);
    
    const int want = std::min(static_cast<int>(std::lround(targetDelayMs_ * fs_ / 1000.0)),
                              static_cast<int>(buf_.size()) - 1);
    if (want == delay_) {
        pendingDelayMs_ = -1.0f;
        return;
    }
    if (dipRemaining_ > 0) {
        // A drag re-arms the pending value instead of re-dipping.
        // Use the validated targetDelayMs_ to prevent out-of-bounds state pollution.
        pendingDelayMs_ = targetDelayMs_;
        return;
    }
    pendingDelayMs_ = targetDelayMs_;
    dipTotal_ = dipRemaining_ = std::max(64, static_cast<int>(0.006 * fs_));
}

void HaasDelay::setWidth(float width) {
    width_ = clampv(width, 0.0f, 2.0f);
    if (std::fabs(width_ - widthCurrent_) < 1e-4f) {
        widthCurrent_ = width_;
        widthRampRemaining_ = 0;
        widthStep_ = 0.0f;
    } else {
        widthRampRemaining_ = std::max(1, static_cast<int>(std::lround(0.020 * fs_)));
        widthStep_ = (width_ - widthCurrent_) / static_cast<float>(widthRampRemaining_);
    }
}

void HaasDelay::setHighpassHz(float hz) {
    hpHz_ = clampv(hz, 80.0f, 250.0f);
    hpCoeffs_ = designRbj(FilterType::HighPass, hpHz_, 0.0, 0.7071, fs_);
}

void HaasDelay::setMonoProtection(bool enabled) {
    monoProtectEnabled_ = enabled;
}

void HaasDelay::process(float* left, float* right, int n) {
    if (delay_ <= 0 && dipRemaining_ <= 0 && pendingDelayMs_ < 0.0f) return;
    if (buf_.empty()) return;
    
    const int size = static_cast<int>(buf_.size());
    const double lfoDepthSamples = 0.05 * fs_ / 1000.0;
    
    for (int i = 0; i < n; ++i) {
        float mid = (left[i] + right[i]) * 0.5f;
        float side = (left[i] - right[i]) * 0.5f;
        
        side = static_cast<float>(biquadTick(hpCoeffs_, hpState_, side));

        if (monoProtectEnabled_) {
            if (++envCounter_ >= 64) {
                envCounter_ = 0;
                midEnv_ += envCoef_ * (mid * mid - midEnv_);
                sideEnv_ += envCoef_ * (side * side - sideEnv_);
                float ratio = sideEnv_ / (midEnv_ + sideEnv_ + 1e-9f);
                if (isMonoActive_ && ratio > 0.08f) isMonoActive_ = false;
                else if (!isMonoActive_ && ratio < 0.02f) isMonoActive_ = true;
            }
            monoFactor_ += 0.001f * ((isMonoActive_ ? 0.0f : 1.0f) - monoFactor_);
        } else {
            monoFactor_ = 1.0f;
        }

        buf_[pos_] = side;
        double lfoOffset = lfoDepthSamples * std::sin(lfoPhase_);
        lfoPhase_ += lfoInc_;
        if (lfoPhase_ >= kTwoPi) lfoPhase_ -= kTwoPi;

        double readPos = pos_ - delay_ - lfoOffset;
        readPos = std::fmod(readPos, (double)size);
        if (readPos < 0.0) readPos += size;
        
        int i1 = static_cast<int>(readPos);
        double frac = readPos - i1;
        int i0 = (i1 - 1 + size) % size;
        int i2 = (i1 + 1) % size;
        int i3 = (i1 + 2) % size;
        
        float y0 = buf_[i0], y1 = buf_[i1], y2 = buf_[i2], y3 = buf_[i3];
        double c0 = y1;
        double c1 = 0.5 * (y2 - y0);
        double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        float delayedSide = static_cast<float>(c0 + frac * (c1 + frac * (c2 + frac * c3)));

        if (++pos_ == size) pos_ = 0;

        if (dipRemaining_ > 0) {
            // 6 ms dip: 1 -> 0 over the first quarter, silent while the delay switches, 0 -> 1.
            const float u = 1.0f - static_cast<float>(dipRemaining_) / static_cast<float>(dipTotal_);
            if (u >= 0.25f && pendingDelayMs_ >= 0.0f) {
                delay_ = std::min(static_cast<int>(std::lround(clampv(pendingDelayMs_, 0.0f, 20.0f) * fs_ / 1000.0)), size - 1);
                pendingDelayMs_ = -1.0f;
            }
            delayedSide *= u < 0.25f ? 1.0f - 4.0f * u : (u > 0.75f ? 4.0f * u - 3.0f : 0.0f);
            if (--dipRemaining_ == 0) dipTotal_ = 0;
        } else if (pendingDelayMs_ >= 0.0f) {
            setDelayMs(pendingDelayMs_);
        }

        if (widthRampRemaining_ > 0) {
            widthCurrent_ += widthStep_;
            --widthRampRemaining_;
        } else {
            widthCurrent_ = width_;
        }

        float finalSide = (side * (1.0f - monoFactor_) + delayedSide * monoFactor_) * widthCurrent_;
        left[i] = mid + finalSide;
        right[i] = mid - finalSide;
    }
}

// ---- DynamicBass ----------------------------------------------------------------------------------

void DynamicBass::prepare(double sampleRate) {
    fs_ = sampleRate;
    envCoef_ = onePoleCoef(0.3, fs_);     // programme envelopes
    smoothCoef_ = onePoleCoef(0.15, fs_ / 64.0);  // per-update smoothing (updates every 64 samples)
    set(s_);
    reset();
}

void DynamicBass::reset() {
    for (BiquadState* s : {&detLpS_, &detHpS_, &shelfL_, &shelfR_, &lowMidL_, &lowMidR_, &presL_, &presR_}) s->reset();
    bassEnv_ = fullEnv_ = 0.0;
    boostDb_ = 0.0;
    appliedBoostDb_ = -1.0;
    counter_ = 0;
}

void DynamicBass::set(const Settings& s) {
    s_ = s;
    s_.bassHz = clampv(s_.bassHz, 30.0f, 200.0f);
    s_.bassQ = clampv(s_.bassQ, 0.2f, 1.2f);
    s_.lowMidHz = clampv(s_.lowMidHz, 150.0f, 1200.0f);
    s_.presenceHz = clampv(s_.presenceHz, 2000.0f, 10000.0f);
    s_.strength = clampv(s_.strength, 0.0f, 1.0f);
    detLp_ = designRbj(FilterType::LowPass, s_.bassHz * 1.5, 0.0, 0.7071, fs_);
    detHp_ = designRbj(FilterType::HighPass, 25.0, 0.0, 0.7071, fs_);
    appliedBoostDb_ = -1.0;  // force coefficient refresh
}

void DynamicBass::updateShelf(double boostDb) {
    shelf_ = designRbj(FilterType::LowShelf, s_.bassHz, boostDb, s_.bassQ + 0.2, fs_);
    // Keep the overall balance: a little low-mid dip and presence lift that scale with the boost.
    lowMid_ = designRbj(FilterType::Peak, s_.lowMidHz, -0.25 * boostDb * clampv(s_.lowMidTrim, 0.0f, 1.2f), 0.8, fs_);
    presence_ = designRbj(FilterType::HighShelf, s_.presenceHz, 0.15 * boostDb * clampv(s_.presenceTrim, 0.0f, 1.2f),
                          0.7071, fs_);
    appliedBoostDb_ = boostDb;
}

void DynamicBass::process(float* left, float* right, int n) {
    const double maxBoost = 16.0 * s_.strength;
    for (int i = 0; i < n; ++i) {
        const double mono = 0.5 * (left[i] + right[i]);
        const double bass = biquadTick(detLp_, detLpS_, biquadTick(detHp_, detHpS_, mono));
        bassEnv_ = envCoef_ * bassEnv_ + (1.0 - envCoef_) * bass * bass;
        fullEnv_ = envCoef_ * fullEnv_ + (1.0 - envCoef_) * mono * mono;
        if (++counter_ >= 64) {
            counter_ = 0;
            // Bass share of the programme: ~0.5 for bass-heavy music, ~0.05 for thin material.
            // Half the boost is always there; the other half comes in as the bass thins out.
            const double share = fullEnv_ > 1e-12 ? bassEnv_ / fullEnv_ : 0.5;
            const double want = maxBoost * (0.5 + 0.5 * clampv(1.0 - share / 0.5, 0.0, 1.0));
            boostDb_ = smoothCoef_ * boostDb_ + (1.0 - smoothCoef_) * want;
            if (std::fabs(boostDb_ - appliedBoostDb_) > 0.1) updateShelf(boostDb_);
        }
        if (appliedBoostDb_ < 0.0) updateShelf(0.0);
        double l = biquadTick(shelf_, shelfL_, left[i]);
        double r = biquadTick(shelf_, shelfR_, right[i]);
        l = biquadTick(lowMid_, lowMidL_, l);
        r = biquadTick(lowMid_, lowMidR_, r);
        left[i] = static_cast<float>(biquadTick(presence_, presL_, l));
        right[i] = static_cast<float>(biquadTick(presence_, presR_, r));
    }
}

// ---- AnalogColor --------------------------------------------------------------------------------

void AnalogColor::prepare(double sampleRate) {
    fs_ = sampleRate;
    tone_.prepare(fs_);
    tone_.setMatched(true);
    setLevel(level_);
}

void AnalogColor::reset() { tone_.reset(); }

void AnalogColor::setLevel(int level) {
    level_ = clampv(level, 0, 2);
    static constexpr float kDrive[3] = {0.12f, 0.25f, 0.45f};
    static constexpr float kLowDb[3] = {1.0f, 1.8f, 2.5f};
    static constexpr float kHighDb[3] = {-0.5f, -1.0f, -1.6f};
    drive_ = kDrive[level_];
    EqBand bands[2];
    bands[0] = {true, FilterType::LowShelf, 110.0f, kLowDb[level_], 0.6f, 0};
    bands[1] = {true, FilterType::HighShelf, 9000.0f, kHighDb[level_], 0.7f, 0};
    tone_.setBands(bands, 2);
}

void AnalogColor::process(float* left, float* right, int n) {
    // Asymmetric soft saturation (adds mostly 2nd harmonic), level-compensated so that the
    // effect colours rather than louder-sounds-better.
    const float k = drive_ * 4.0f;
    const float norm = 1.0f / std::tanh(k);
    const float bias = 0.15f * drive_;
    const float biasOut = std::tanh(k * bias) * norm;
    auto shape = [&](float x) {
        const float y = std::tanh(k * (x + bias)) * norm - biasOut;
        return 0.6f * x + 0.4f * y;
    };
    for (int i = 0; i < n; ++i) {
        left[i] = shape(left[i]);
        right[i] = shape(right[i]);
    }
    tone_.process(left, right, n);
}

// ---- SpeakerVoicing -------------------------------------------------------------------------------

namespace {

struct ProfileBand {
    FilterType type;
    float freq, gainDb, q;
};

// Typical responses of each speaker class and what restores a natural balance on them. High-
// passes keep energy the driver can't reproduce from turning into distortion; gains are at
// 100 % strength.
const std::vector<ProfileBand>& profileBands(int profile) {
    static const std::vector<ProfileBand> kProfiles[SpeakerVoicing::kMeasured] = {
        // Phone: micro-speakers roll off below ~300 Hz; lift what they can still play and the
        // presence region they mask, without pushing the 1-2 kHz "honk".
        {{FilterType::HighPass, 150.0f, 0.0f, 0.7071f},
         {FilterType::Peak, 320.0f, 3.0f, 1.0f},
         {FilterType::Peak, 1500.0f, -1.5f, 1.4f},
         {FilterType::Peak, 3000.0f, 2.0f, 1.2f},
         {FilterType::HighShelf, 10000.0f, 1.5f, 0.7071f}},
        // Tablet: larger drivers and cavities, a little more low end to work with.
        {{FilterType::HighPass, 110.0f, 0.0f, 0.7071f},
         {FilterType::Peak, 220.0f, 3.0f, 0.9f},
         {FilterType::Peak, 2500.0f, 1.5f, 1.2f},
         {FilterType::HighShelf, 9000.0f, 1.5f, 0.7071f}},
        // Laptop: down-firing drivers sound muffled and boxy; trim the box, open the top.
        {{FilterType::HighPass, 120.0f, 0.0f, 0.7071f},
         {FilterType::Peak, 250.0f, 2.5f, 1.0f},
         {FilterType::Peak, 600.0f, -2.0f, 1.4f},
         {FilterType::Peak, 4000.0f, -1.0f, 2.0f},
         {FilterType::HighShelf, 8000.0f, 2.5f, 0.7071f}},
        // Small Bluetooth speaker: passive-radiator bass bump, recessed treble.
        {{FilterType::HighPass, 55.0f, 0.0f, 0.7071f},
         {FilterType::Peak, 120.0f, -1.5f, 1.2f},
         {FilterType::Peak, 2500.0f, -1.0f, 1.5f},
         {FilterType::HighShelf, 8000.0f, 2.0f, 0.7071f}},
        // Large Bluetooth / home speaker: boomy upper bass is the usual problem.
        {{FilterType::HighPass, 30.0f, 0.0f, 0.7071f},
         {FilterType::Peak, 100.0f, -2.0f, 1.0f},
         {FilterType::Peak, 300.0f, -1.0f, 1.2f},
         {FilterType::HighShelf, 9000.0f, 1.0f, 0.7071f}},
        // Car: cabin gain piles up the low end; door speakers fire at the knees, so lift the
        // presence region to bring the image up to the dashboard.
        {{FilterType::Peak, 60.0f, -2.0f, 0.8f},
         {FilterType::Peak, 160.0f, -2.5f, 1.4f},
         {FilterType::Peak, 2500.0f, 2.0f, 1.0f},
         {FilterType::HighShelf, 8000.0f, 1.5f, 0.7071f}},
        // TV: thin, rear-firing speakers; body and dialogue clarity.
        {{FilterType::HighPass, 80.0f, 0.0f, 0.7071f},
         {FilterType::Peak, 160.0f, 2.0f, 1.0f},
         {FilterType::Peak, 2500.0f, 2.0f, 1.2f},
         {FilterType::HighShelf, 10000.0f, 1.0f, 0.7071f}},
    };
    return kProfiles[clampv(profile, 0, SpeakerVoicing::kMeasured - 1)];
}

}  // namespace

void SpeakerVoicing::prepare(double sampleRate) {
    fs_ = sampleRate;
    eq_.prepare(sampleRate);
    eq_.setMatched(true);
    rebuild();
}

void SpeakerVoicing::reset() { eq_.reset(); }

void SpeakerVoicing::setProfile(int profile, float strength) {
    profile_ = clampv(profile, 0, kProfileCount - 1);
    strength_ = clampv(strength, 0.0f, 1.0f);
    rebuild();
}

void SpeakerVoicing::setMeasured(const EqBand* bands, int n, float preampDb) {
    measured_.assign(bands, bands + clampv(n, 0, kMaxBands));
    measuredPreampDb_ = preampDb;
    if (profile_ == kMeasured) rebuild();
}

void SpeakerVoicing::rebuild() {
    std::vector<EqBand> bands;
    float preamp = 0.0f;
    if (profile_ == kMeasured) {
        bands = measured_;
        for (EqBand& b : bands)
            if (filterHasGain(b.type)) b.gainDb *= strength_;
        preamp = measuredPreampDb_ * strength_;
    } else {
        for (const ProfileBand& p : profileBands(profile_)) {
            if (p.freq >= 0.45 * fs_) continue;
            bands.push_back({true, p.type, p.freq, p.gainDb * strength_, p.q, 0});
        }
    }
    eq_.setBands(bands.data(), static_cast<int>(bands.size()));
    eq_.setPreampDb(preamp);
}

void SpeakerVoicing::process(float* left, float* right, int n) { eq_.process(left, right, n); }

}  // namespace rfx::fx
