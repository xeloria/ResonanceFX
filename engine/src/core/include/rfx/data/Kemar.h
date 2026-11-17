// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// MIT KEMAR head-related impulse responses, full sphere (see core/src/data/KemarHrir.cpp).

#include <cstdint>

namespace rfx {
namespace kemar {

constexpr int kElevations = 14;      // -40, -30, ..., 90 degrees
constexpr int kElevationMin = -40;   // degrees
constexpr int kElevationStep = 10;   // degrees
constexpr int kAzimuths = 37;        // 0, 5, ..., 180 degrees; positive = source to the right
constexpr int kTaps = 128;
constexpr double kSampleRate = 44100.0;

/// kHrir[e][a][ear][n]: ear 0 = left, 1 = right, source at azimuth a*5 degrees to the right
/// and elevation kElevationMin + e*kElevationStep. Signed 16-bit, scale by 1/32768.
/// One pinna is used for both ears (the archive's recommended symmetric arrangement), so the
/// median plane is exactly symmetric and sources on the left use the mirrored, ear-swapped
/// response.
extern const int16_t kHrir[kElevations][kAzimuths][2][kTaps];

}  // namespace kemar
}  // namespace rfx
