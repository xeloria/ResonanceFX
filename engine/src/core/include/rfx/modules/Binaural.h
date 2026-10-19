// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Binaural loudspeaker virtualisation with measured head-related transfer functions and
// optional head tracking.

#include <vector>

#include "rfx/dsp/Common.h"

namespace rfx {

/// Renders the stereo signal as two virtual loudspeakers (+/- angle) plus two early room
/// reflections, using the MIT KEMAR head-related impulse responses (Gardner & Martin 1994).
///  - The HRIR set is diffuse-field equalised (the average response over all directions is
///    removed), so the timbre stays neutral and only the directional cues remain.
///  - Each HRIR is split into a minimum-phase filter and a pure interaural delay; directions
///    between the 5-degree measurement grid are interpolated (filters linearly, delays
///    separately), which is artefact-free for minimum-phase responses.
///  - With head tracking, virtual sources stay fixed in the room as the head turns
///    (source azimuth - head yaw); filter changes are crossfaded per 32-sample block.
class HrtfSpatializer {
public:
    struct Settings {
        float angleDeg = 30.0f, room = 0.3f, yawDeg = 0.0f;
        bool tracking = true;
    };
    void prepare(double sampleRate);  // allocates and builds the processed HRIR table
    void reset();
    void set(const Settings& s) { s_ = s; }
    void process(float* left, float* right, int n);

    /// Processed response for a direction (diagnostics/tests): fills taps per ear, returns
    /// the delays in samples.
    void response(float azimuthDeg, std::vector<float>& left, std::vector<float>& right, float* delayL,
                  float* delayR) const;
    int taps() const { return taps_; }

private:
    static constexpr int kGrid = 37;   // 0..180 degrees, 5-degree steps
    static constexpr int kSources = 4; // left speaker, right speaker, two reflections
    static constexpr int kBlock = 32;
    struct Filter {
        std::vector<float> h[2];
        int delay[2] = {0, 0};
        float az = 1e9f;
    };
    void interpolate(float azimuthDeg, Filter& out) const;
    double renderSample(const Filter& f, int src, int ear, int64_t t) const;

    Settings s_;
    double fs_ = 48000.0;
    int taps_ = 128;
    std::vector<float> table_;   // [kGrid][2][taps_]
    int delays_[kGrid][2] = {};
    float norm_ = 0.5f;
    std::vector<float> hist_[kSources];
    int histMask_ = 0;
    int64_t t_ = 0;
    Filter cur_[kSources], next_[kSources];
};

}  // namespace rfx
