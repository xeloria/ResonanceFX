// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../Engine.h"
#include "../fx/Abx.h"
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
    void passThrough(const audio_buffer_t* in, audio_buffer_t* out, int frames, bool accumulate,
                     uint8_t inFormat, uint8_t outFormat, uint32_t channelCount);
    /// TPDF dither + first-order error-feedback noise shaping, quantised to 16 bit.
    inline int16_t quantize16(float x, unsigned channel) {
        const float v = x * 32768.0f - ditherErr_[channel];
        ditherState_ = ditherState_ * 1664525u + 1013904223u;
        const float a = static_cast<float>(ditherState_ >> 8) * (1.0f / 16777216.0f);
        ditherState_ = ditherState_ * 1664525u + 1013904223u;
        const float b = static_cast<float>(ditherState_ >> 8) * (1.0f / 16777216.0f);
        float q = std::floor(v + (a - b) + 0.5f);
        q = q < -32768.0f ? -32768.0f : (q > 32767.0f ? 32767.0f : q);
        ditherErr_[channel] = clampv(q - v, -4.0f, 4.0f);
        return static_cast<int16_t>(q);
    }

    /// Everything the try-lock-failed pass-through path needs, published under the mutex so the
    /// audio thread never reads mutex-owned config_ while a command thread mutates it.
    struct BypassSnapshot {
        audio_buffer_t inBuffer{}, outBuffer{};
        bool inFallback = false, outFallback = false;
        bool enabled = false, disabled = true;
        uint8_t inFormat = 0, outFormat = 0, accessMode = 0;
        uint32_t channels = 2;
    };
    void publishBypass();

    int32_t setAbx(int32_t id, int32_t value);

    std::mutex mutex_;
    Engine engine_;
    // Listening test: the reference rendering receives every parameter (with the effect under
    // test forced off) but only processes while a test runs.
    Engine reference_;
    fx::AbxSwitch abx_;
    bool abxOn_ = false;
    int32_t abxExclude_ = 0;
    std::map<int32_t, int32_t> lastScalar_;  // first int of every scalar parameter, to restore exclusions
    std::vector<float> refLeft_, refRight_;
    effect_config_t config_{};
    uint32_t channels_ = 2;
    bool enabled_ = false;
    Disable disable_ = Disable::kUnknown;
    std::string disableMessage_;
    std::vector<float> left_, right_;
    // Dither/noise-shaping state for 16-bit output (one error-feedback state per channel).
    uint32_t ditherState_ = 0x12345678u;
    float ditherErr_[2] = {0.0f, 0.0f};
    // Published by the command path for the lock-free pass-through; use atomic_load/store
    // (GCC 12's atomic<shared_ptr> isn't available in the NDK's and host libstdc++).
    std::shared_ptr<const BypassSnapshot> bypass_;
};

}  // namespace rfx::android
