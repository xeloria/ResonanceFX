// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "../Engine.h"
#include "android_effect_abi.h"

namespace rfx::android {

/// One effect instance as seen by the Android effect framework: command handling, stream
/// configuration and sample-format conversion around the Engine.
///
/// Threading: commands arrive on a binder thread, process() on the audio thread. Both take
/// the same mutex, but process() only ever try-locks; while a command is running (e.g. a long
/// impulse response being prepared) the audio passes through unprocessed instead of blocking.
class EffectContext {
public:
    EffectContext();

    int32_t command(uint32_t code, uint32_t size, void* data, uint32_t* replySize, void* reply);
    int32_t process(audio_buffer_t* in, audio_buffer_t* out);

private:
    enum class Disable : int32_t { kUnknown = -1, kNone = 0, kFrameCount = 1, kSampleRate = 2, kChannels = 3, kFormat = 4 };

    void configure(const effect_config_t& cfg);
    int32_t setParameter(const effect_param_t* p, uint32_t cmdSize);
    int32_t getParameter(const effect_param_t* p, effect_param_t* reply, uint32_t* replySize);
    void disable(Disable reason, std::string message);

    void readInput(const audio_buffer_t* in, int frames);
    void writeOutput(const audio_buffer_t* in, audio_buffer_t* out, int frames, bool accumulate);
    void passThrough(const audio_buffer_t* in, audio_buffer_t* out, int frames, bool accumulate);

    std::mutex mutex_;
    Engine engine_;
    effect_config_t config_{};
    uint32_t channels_ = 2;
    bool enabled_ = false;
    Disable disable_ = Disable::kUnknown;
    std::string disableMessage_;
    std::vector<float> left_, right_;
};

}  // namespace rfx::android
