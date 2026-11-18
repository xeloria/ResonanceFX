// SPDX-License-Identifier: Apache-2.0
// Placeholder for AOSP's system/audio_effects/aidl_effects_utils.h.
//
// EffectImpl/EffectContext use the AIDL version constants and EventFlag bits below; the real
// header also pulls in audio_utils template helpers that only the built-in effects need.
#pragma once

#include <cstdint>

namespace aidl::android::hardware::audio::effect {

/// First AIDL version with IEffect::reopen.
static constexpr int32_t kReopenSupportedVersion = 2;
/// First AIDL version with State::DRAINING.
static constexpr int32_t kDrainSupportedVersion = 3;
/// First AIDL version that allows destroying an effect in any state.
static constexpr int32_t kDestroyAnyStateSupportedVersion = 3;

/// Deprecated client->effect "data written" flag (HAL version 1).
static constexpr uint32_t kEventFlagNotEmpty = 0x1;
/// Effect->client: the data FMQ needs to be reopened.
static constexpr uint32_t kEventFlagDataMqUpdate = 0x1 << 10;
/// Client->effect: the data FMQ is no longer empty.
static constexpr uint32_t kEventFlagDataMqNotEmpty = 0x1 << 11;

}  // namespace aidl::android::hardware::audio::effect
