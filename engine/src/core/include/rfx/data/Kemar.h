// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// MIT KEMAR head-related impulse responses, horizontal plane (see core/src/data/KemarHrir.cpp).

namespace rfx {
namespace kemar {

constexpr int kAzimuths = 37;      // 0, 5, ..., 180 degrees; positive = source to the right
constexpr int kTaps = 128;
constexpr double kSampleRate = 44100.0;

/// kHrir[a][ear][n]: ear 0 = left, 1 = right, source at a*5 degrees to the right.
/// Sources on the left use the mirrored (ear-swapped) response.
extern const float kHrir[kAzimuths][2][kTaps];

}  // namespace kemar
}  // namespace rfx
