// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Binaural loudspeaker virtualisation with measured head-related transfer functions and
// optional 3DoF head tracking.

#include <memory>
#include <vector>

#include "rfx/dsp/Common.h"

namespace rfx {

/// Renders the stereo signal as two virtual loudspeakers (+/- angle) plus two early room
/// reflections, using the MIT KEMAR head-related impulse responses (Gardner & Martin 1994,
/// 710 positions, elevations -40..90 degrees).
///  - The HRIR set is diffuse-field equalised (the average response over all directions is
///    removed), so the timbre stays neutral and only the directional cues remain.
///  - Each HRIR is split into a minimum-phase filter and a pure interaural delay; directions
///    between the 5-degree azimuth / 10-degree elevation measurement grid are interpolated
///    bilinearly (filters linearly, delays separately), which is artefact-free for
///    minimum-phase responses.
///  - With head tracking, virtual sources stay fixed in the room as the head turns, tilts and
///    rolls: the source direction is rotated by the inverse head orientation (a quaternion) and
///    rendered at its resulting azimuth and elevation. Filter changes are crossfaded per
///    32-sample block.
class HrtfSpatializer {
public:
    struct Settings {
        float angleDeg = 30.0f, room = 0.3f, yawDeg = 0.0f;
        /// Unit quaternion (w, x, y, z) of the head in the world, x right, y forward, z up.
        /// Used when `orientation` is set; otherwise `yawDeg` (the legacy yaw-only path).
        float quatW = 1.0f, quatX = 0.0f, quatY = 0.0f, quatZ = 0.0f;
        bool tracking = true, orientation = false;
    };
    void prepare(double sampleRate);  // attaches the processed HRIR table for this rate
    void reset();
    void set(const Settings& s) { s_ = s; }
    void process(float* left, float* right, int n);

    /// Impulse response the renderer produces for `settings` (used to fit the compensation EQ
    /// without building a second renderer). Fills `n` samples per ear.
    void impulseResponse(const Settings& settings, std::vector<float>& left, std::vector<float>& right,
                         int n) const;

    /// World direction -> head-frame direction for a unit quaternion (head -> world). Exposed
    /// for tests: identity maps the direction unchanged, a yaw of +30 degrees (head turned
    /// left) moves a frontal source 30 degrees to the right, and so on.
    static void headDirection(float azimuthDeg, float elevationDeg, float qw, float qx, float qy,
                              float qz, float* outAzimuthDeg, float* outElevationDeg);

    /// Processed response for a direction (diagnostics/tests): fills taps per ear, returns
    /// the delays in samples.
    void response(float azimuthDeg, float elevationDeg, std::vector<float>& left,
                  std::vector<float>& right, float* delayL, float* delayR) const;
    int taps() const { return taps_; }

private:
    static constexpr int kAzGrid = 37;   // 0..180 degrees, 5-degree steps
    static constexpr int kElGrid = 14;   // -40..90 degrees, 10-degree steps
    static constexpr int kSources = 4;   // left speaker, right speaker, two reflections
    static constexpr int kBlock = 32;

    /// The sample-rate-dependent renderer data. It is immutable and built once per rate, then
    /// shared by every instance: 14 elevations x 37 azimuths of minimum-phase design is far too
    /// expensive to repeat for every effect instance (and every ABX reference engine).
    struct Table {
        int taps = 128;
        int histMask = 0;
        float norm = 0.5f;
        std::vector<float> hrir;  // [elevation][azimuth][2][taps], reversed for dot products
        int delay[kElGrid][kAzGrid][2] = {};
    };
    struct Filter {
        std::vector<float> h[2];
        int delay[2] = {0, 0};
        float az = 1e9f, el = 1e9f;
    };
    static std::shared_ptr<const Table> sharedTable(double sampleRate);
    static void buildTable(double fs, Table& table);
    static void interpolateInto(const Table& table, float azimuthDeg, float elevationDeg, Filter& out);
    void interpolate(float azimuthDeg, float elevationDeg, Filter& out) const;
    double renderSample(const Filter& f, int src, int ear, int64_t t) const;

    Settings s_;
    double fs_ = 48000.0;
    int taps_ = 128;
    std::shared_ptr<const Table> table_;
    std::vector<float> hist_[kSources];
    int histMask_ = 0;
    int64_t t_ = 0;
    Filter cur_[kSources], next_[kSources];
};

}  // namespace rfx
