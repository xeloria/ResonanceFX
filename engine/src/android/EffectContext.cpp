// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#include "EffectContext.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>

#include "../Log.h"
#include "../Protocol.h"

#ifndef RFX_VERSION_CODE
#define RFX_VERSION_CODE 0
#endif
#ifndef RFX_VERSION_NAME
#define RFX_VERSION_NAME "dev"
#endif

#if defined(__aarch64__)
#define RFX_ARCH "ARM64"
#elif defined(__arm__)
#define RFX_ARCH "ARM"
#elif defined(__x86_64__)
#define RFX_ARCH "x86_64"
#elif defined(__i386__)
#define RFX_ARCH "x86"
#else
#define RFX_ARCH "unknown"
#endif

namespace rfx::android {

using namespace rfx::proto;

namespace {

constexpr uint32_t kMaxChannels = 28;  // AOSP FCC_LIMIT

uint32_t channelCount(uint32_t mask) {
    // Index masks carry their representation in the top two bits.
    if ((mask >> 30) == 2) return static_cast<uint32_t>(__builtin_popcount(mask & 0x3FFFFFFFu));
    return static_cast<uint32_t>(__builtin_popcount(mask));
}

bool supportedFormat(uint8_t f) {
    return f == AUDIO_FORMAT_PCM_16_BIT || f == AUDIO_FORMAT_PCM_32_BIT || f == AUDIO_FORMAT_PCM_8_24_BIT ||
           f == AUDIO_FORMAT_PCM_24_BIT_PACKED || f == AUDIO_FORMAT_PCM_FLOAT;
}

size_t sampleBytes(uint8_t f) {
    return f == AUDIO_FORMAT_PCM_16_BIT ? 2 : f == AUDIO_FORMAT_PCM_24_BIT_PACKED ? 3 : 4;
}

inline float load(const uint8_t* base, uint8_t f, size_t i) {
    switch (f) {
        case AUDIO_FORMAT_PCM_16_BIT: return reinterpret_cast<const int16_t*>(base)[i] * (1.0f / 32768.0f);
        case AUDIO_FORMAT_PCM_32_BIT:
            return static_cast<float>(reinterpret_cast<const int32_t*>(base)[i] * (1.0 / 2147483648.0));
        case AUDIO_FORMAT_PCM_8_24_BIT: return reinterpret_cast<const int32_t*>(base)[i] * (1.0f / 8388608.0f);
        case AUDIO_FORMAT_PCM_24_BIT_PACKED: {
            const uint8_t* p = base + i * 3;
            const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(p[0]) << 8 | static_cast<uint32_t>(p[1]) << 16 |
                                                   static_cast<uint32_t>(p[2]) << 24) >> 8;
            return v * (1.0f / 8388608.0f);
        }
        default: return reinterpret_cast<const float*>(base)[i];
    }
}

inline float saturate(float v) {
    if (!(v == v)) return 0.0f;  // NaN
    return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
}

inline void store(uint8_t* base, uint8_t f, size_t i, float v, bool accumulate) {
    if (accumulate) v += load(base, f, i);
    v = saturate(v);
    switch (f) {
        case AUDIO_FORMAT_PCM_16_BIT: reinterpret_cast<int16_t*>(base)[i] = static_cast<int16_t>(lrintf(v * 32767.0f)); break;
        case AUDIO_FORMAT_PCM_32_BIT: reinterpret_cast<int32_t*>(base)[i] = static_cast<int32_t>(llrint(v * 2147483647.0)); break;
        case AUDIO_FORMAT_PCM_8_24_BIT: reinterpret_cast<int32_t*>(base)[i] = static_cast<int32_t>(lrintf(v * 8388607.0f)); break;
        case AUDIO_FORMAT_PCM_24_BIT_PACKED: {
            const int32_t q = static_cast<int32_t>(lrintf(v * 8388607.0f));
            uint8_t* p = base + i * 3;
            p[0] = static_cast<uint8_t>(q & 0xFF);
            p[1] = static_cast<uint8_t>((q >> 8) & 0xFF);
            p[2] = static_cast<uint8_t>((q >> 16) & 0xFF);
            break;
        }
        default: reinterpret_cast<float*>(base)[i] = v; break;
    }
}

void merge(buffer_config_t& dst, const buffer_config_t& src) {
    if (src.mask & EFFECT_CONFIG_BUFFER) dst.buffer = src.buffer;
    if (src.mask & EFFECT_CONFIG_SMP_RATE) dst.samplingRate = src.samplingRate;
    if (src.mask & EFFECT_CONFIG_CHANNELS) dst.channels = src.channels;
    if (src.mask & EFFECT_CONFIG_FORMAT) dst.format = src.format;
    if (src.mask & EFFECT_CONFIG_ACC_MODE) dst.accessMode = src.accessMode;
    if (src.mask & EFFECT_CONFIG_PROVIDER) dst.bufferProvider = src.bufferProvider;
    dst.mask |= src.mask;
}

uint32_t alignedParamSize(uint32_t psize) { return (psize + 3u) & ~3u; }

}  // namespace

EffectContext::EffectContext() {
    left_.assign(Engine::kMaxBlock, 0.0f);
    right_.assign(Engine::kMaxBlock, 0.0f);
    refLeft_.assign(Engine::kMaxBlock, 0.0f);
    refRight_.assign(Engine::kMaxBlock, 0.0f);
    reference_.setModulesOff(true);
}

void EffectContext::disable(Disable reason, std::string message) {
    disable_ = reason;
    disableMessage_ = std::move(message);
    if (reason != Disable::kNone) RFX_LOGE("Stream rejected: %s", disableMessage_.c_str());
}

void EffectContext::configure(const effect_config_t& cfg) {
    merge(config_.inputCfg, cfg.inputCfg);
    merge(config_.outputCfg, cfg.outputCfg);
    const buffer_config_t& in = config_.inputCfg;
    const buffer_config_t& out = config_.outputCfg;
    RFX_LOGI("Config: in %u Hz ch 0x%x fmt %u, out %u Hz ch 0x%x fmt %u acc %u, frames %zu/%zu", in.samplingRate,
             in.channels, in.format, out.samplingRate, out.channels, out.format, out.accessMode,
             in.buffer.frameCount, out.buffer.frameCount);

    // AIDL HALs configure with frameCount 0 and pass the real size with every buffer.
    if (in.buffer.frameCount && out.buffer.frameCount && in.buffer.frameCount != out.buffer.frameCount)
        return disable(Disable::kFrameCount, "Input and output frame count differ");
    if (in.samplingRate == 0 || in.samplingRate != out.samplingRate)
        return disable(Disable::kSampleRate, "Input and output sample rates differ");
    const uint32_t ch = channelCount(in.channels);
    if (ch == 0 || ch > kMaxChannels || ch != channelCount(out.channels))
        return disable(Disable::kChannels, "Unsupported channel layout");
    if (!supportedFormat(in.format) || !supportedFormat(out.format))
        return disable(Disable::kFormat, "Unsupported sample format");

    channels_ = ch;
    if (std::fabs(engine_.sampleRate() - in.samplingRate) > 0.5) {
        engine_.prepare(in.samplingRate);
        reference_.prepare(in.samplingRate);
        abx_.prepare(in.samplingRate);
    } else {
        engine_.reset();
        reference_.reset();
    }
    disable(Disable::kNone, "");
    RFX_LOGI("Configured: %u channels at %u Hz", ch, in.samplingRate);
}

int32_t EffectContext::command(uint32_t code, uint32_t size, void* data, uint32_t* replySize, void* reply) {
    const uint32_t replyCap = replySize ? *replySize : 0;
    auto replyStatus = [&](int32_t status) {
        if (reply && replyCap >= sizeof(int32_t)) *static_cast<int32_t*>(reply) = status;
    };
    std::lock_guard<std::mutex> lock(mutex_);
    switch (code) {
        case EFFECT_CMD_INIT:
            if (!reply || replyCap != sizeof(int32_t)) return -EINVAL;
            replyStatus(0);
            return 0;
        case EFFECT_CMD_SET_CONFIG:
            if (!data || size < sizeof(effect_config_t) || !reply || replyCap != sizeof(int32_t)) return -EINVAL;
            configure(*static_cast<const effect_config_t*>(data));
            replyStatus(0);
            return 0;
        case EFFECT_CMD_GET_CONFIG:
            if (!reply || replyCap != sizeof(effect_config_t)) return -EINVAL;
            *static_cast<effect_config_t*>(reply) = config_;
            return 0;
        case EFFECT_CMD_RESET:
#ifdef RFX_DIAG
            RFX_LOGI("diag cmd RESET");
#endif
            engine_.reset();
            reference_.reset();
            replyStatus(0);
            return 0;
        case EFFECT_CMD_ENABLE:
        case EFFECT_CMD_DISABLE:
            if (!reply || replyCap != sizeof(int32_t)) return -EINVAL;
            enabled_ = code == EFFECT_CMD_ENABLE;
#ifdef RFX_DIAG
            RFX_LOGI("diag cmd %s", enabled_ ? "ENABLE" : "DISABLE");
#endif
            if (enabled_) {
                engine_.reset();
                reference_.reset();
            }
            replyStatus(0);
            return 0;
        case EFFECT_CMD_SET_PARAM: {
            if (!data || size < sizeof(effect_param_t) || !reply || replyCap != sizeof(int32_t)) return -EINVAL;
            const int32_t status = setParameter(static_cast<const effect_param_t*>(data), size);
            replyStatus(status);
            return status;
        }
        case EFFECT_CMD_GET_PARAM:
            if (!data || size < sizeof(effect_param_t) + sizeof(int32_t) || !reply || replyCap < sizeof(effect_param_t))
                return -EINVAL;
            return getParameter(static_cast<const effect_param_t*>(data), static_cast<effect_param_t*>(reply), replySize);
        // Routing and mode notifications: nothing depends on them, but some vendor frameworks
        // treat an error here as a broken effect.
        case EFFECT_CMD_SET_DEVICE:
        case EFFECT_CMD_SET_INPUT_DEVICE:
        case EFFECT_CMD_SET_AUDIO_MODE:
        case EFFECT_CMD_SET_AUDIO_SOURCE:
        case EFFECT_CMD_SET_PARAM_COMMIT:
            replyStatus(0);
            return 0;
        default:
            return -EINVAL;
    }
}

int32_t EffectContext::setParameter(const effect_param_t* p, uint32_t cmdSize) {
    if (p->psize < sizeof(int32_t)) return -EINVAL;
    const uint32_t valueOffset = alignedParamSize(p->psize);
    if (static_cast<uint64_t>(sizeof(effect_param_t)) + valueOffset + p->vsize > cmdSize) return -EINVAL;
    int32_t id;
    std::memcpy(&id, p->data, sizeof(id));
    const auto* value = reinterpret_cast<const uint8_t*>(p->data) + valueOffset;
    if (id == kAbxMode || id == kAbxExclude || id == kAbxSelect) {
        if (p->vsize < sizeof(int32_t)) return -EINVAL;
        int32_t v;
        std::memcpy(&v, value, sizeof(v));
        return setAbx(id, v);
    }
    const int32_t status = engine_.setParameter(id, value, p->vsize);
    if (status != 0) return status;
    if (p->vsize >= sizeof(int32_t) && p->vsize <= 16) {
        int32_t first;
        std::memcpy(&first, value, sizeof(first));
        lastScalar_[id] = first;
        if (id == abxExclude_) {
            const int32_t off = 0;  // the effect under test stays off in the reference
            reference_.setParameter(id, reinterpret_cast<const uint8_t*>(&off), sizeof(off));
            return status;
        }
    }
    reference_.setParameter(id, value, p->vsize);
    return status;
}

int32_t EffectContext::setAbx(int32_t id, int32_t v) {
    switch (id) {
        case kAbxMode:
            if ((v != 0) != abxOn_) {
                abxOn_ = v != 0;
                if (abxOn_) {
                    reference_.reset();
                    abx_.prepare(engine_.sampleRate());
                }
            }
            return 0;
        case kAbxSelect: abx_.select(v != 0); return 0;
        case kAbxExclude: {
            if (v == abxExclude_) return 0;
            // Restore the previously excluded effect in the reference.
            if (abxExclude_ != 0) {
                const auto it = lastScalar_.find(abxExclude_);
                const int32_t prev = it != lastScalar_.end() ? it->second : 0;
                reference_.setParameter(abxExclude_, reinterpret_cast<const uint8_t*>(&prev), sizeof(prev));
            }
            abxExclude_ = v;
            reference_.setModulesOff(v == 0);
            if (v != 0) {
                const int32_t off = 0;
                reference_.setParameter(v, reinterpret_cast<const uint8_t*>(&off), sizeof(off));
            }
            abx_.reset();
            return 0;
        }
        default: return -EINVAL;
    }
}

int32_t EffectContext::getParameter(const effect_param_t* p, effect_param_t* reply, uint32_t* replySize) {
    if (p->psize < sizeof(int32_t)) return -EINVAL;
    const uint32_t cap = *replySize;
    const uint32_t valueOffset = alignedParamSize(p->psize);
    if (cap < sizeof(effect_param_t) + valueOffset) return -EINVAL;
    std::memcpy(reply, p, sizeof(effect_param_t) + p->psize);
    uint8_t* value = reinterpret_cast<uint8_t*>(reply->data) + valueOffset;
    const uint32_t room = cap - static_cast<uint32_t>(sizeof(effect_param_t)) - valueOffset;

    auto put = [&](const void* src, uint32_t n) -> int32_t {
        if (n > room) return -EINVAL;
        std::memcpy(value, src, n);
        reply->status = 0;
        reply->vsize = n;
        *replySize = static_cast<uint32_t>(sizeof(effect_param_t)) + valueOffset + n;
        return 0;
    };
    auto putInt = [&](int32_t v) { return put(&v, sizeof(v)); };
    // The disable message is informational, so it is truncated to fit; anything else (like the
    // version name) is refused rather than returned incomplete.
    auto putTruncated = [&](const std::string& s) {
        return put(s.data(), std::min<uint32_t>(static_cast<uint32_t>(s.size()), room));
    };

    int32_t id;
    std::memcpy(&id, p->data, sizeof(id));
    switch (id) {
        case kGetEnabled: return putInt(enabled_ ? 1 : 0);
        case kGetConfigured: return putInt(disable_ == Disable::kNone ? 1 : 0);
        case kGetStreaming: {
            using namespace std::chrono;
            const uint64_t now = static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
            const uint64_t last = engine_.lastProcessMs();
            return putInt(last != 0 && now >= last && now - last < 5000 ? 1 : 0);
        }
        case kGetSampleRate: return putInt(static_cast<int32_t>(engine_.sampleRate()));
        case kGetKernelId: return putInt(static_cast<int32_t>(engine_.kernelId()));
        case kGetVersionCode: return putInt(RFX_VERSION_CODE);
        case kGetVersionName: return put(RFX_VERSION_NAME, sizeof(RFX_VERSION_NAME) - 1);
        case kGetDisableReason: return putInt(static_cast<int32_t>(disable_));
        case kGetDisableMessage: return putTruncated(disableMessage_);
        case kGetConfig: {
            uint8_t c[40] = {};
            auto pack = [&c](int at, const buffer_config_t& b) {
                const uint64_t fc = b.buffer.frameCount;
                std::memcpy(c + at, &fc, 8);
                std::memcpy(c + at + 8, &b.samplingRate, 4);
                std::memcpy(c + at + 12, &b.channels, 4);
                c[at + 16] = b.format;
                c[at + 17] = b.accessMode;
                std::memcpy(c + at + 18, &b.mask, 2);
            };
            pack(0, config_.inputCfg);
            pack(20, config_.outputCfg);
            return put(c, sizeof(c));
        }
        case kGetArchitecture: return put(RFX_ARCH, sizeof(RFX_ARCH) - 1);
        case kGetChannelCount: return putInt(static_cast<int32_t>(channels_));
        case kGetAbxDelta:
            return putInt(abxOn_ && abx_.measured() ? static_cast<int32_t>(std::lround(abx_.deltaDb() * 100.0)) : kAbxNotReady);
        case kGetFeatures:
            return putInt(static_cast<int32_t>(kFeatureChannelCount | kFeatureParametricEq | kFeatureLoudness |
                                               kFeatureUserEq | kFeatureGraphicEq | kFeatureMultiband |
                                               kFeatureDynamicEq | kFeatureDeEsser | kFeatureVocal | kFeatureSpatial |
                                               kFeatureTrueStereoKernel | kFeatureSpeakerProfiles |
                                               kFeatureSpeakerProtection | kFeatureCrosstalk |
                                               kFeatureLevelOffset | kFeatureAbx |
                                               kFeatureMeasuredProtection));
        default:
            return -EINVAL;
    }
}

// ---- Audio ---------------------------------------------------------------------------------------

void EffectContext::readInput(const audio_buffer_t* in, int frames) {
    const auto* src = static_cast<const uint8_t*>(in->raw);
    const uint8_t f = config_.inputCfg.format;
    if (channels_ == 1) {
        for (int i = 0; i < frames; ++i) left_[i] = right_[i] = load(src, f, static_cast<size_t>(i));
        return;
    }
    for (int i = 0; i < frames; ++i) {
        const size_t frame = static_cast<size_t>(i) * channels_;
        left_[i] = load(src, f, frame);
        right_[i] = load(src, f, frame + 1);
    }
}

void EffectContext::writeOutput(const audio_buffer_t* in, audio_buffer_t* out, int frames, bool accumulate) {
    auto* dst = static_cast<uint8_t*>(out->raw);
    const uint8_t of = config_.outputCfg.format;
    if (channels_ == 1) {
        for (int i = 0; i < frames; ++i) store(dst, of, static_cast<size_t>(i), 0.5f * (left_[i] + right_[i]), accumulate);
        return;
    }
    // Surround/height channels pass through untouched (already in place when processing in place).
    const bool copyExtra = channels_ > 2 && (in->raw != out->raw || accumulate);
    const auto* src = static_cast<const uint8_t*>(in->raw);
    const uint8_t inf = config_.inputCfg.format;
    for (int i = 0; i < frames; ++i) {
        const size_t frame = static_cast<size_t>(i) * channels_;
        store(dst, of, frame, left_[i], accumulate);
        store(dst, of, frame + 1, right_[i], accumulate);
        if (copyExtra)
            for (uint32_t c = 2; c < channels_; ++c) store(dst, of, frame + c, load(src, inf, frame + c), accumulate);
    }
}

void EffectContext::passThrough(const audio_buffer_t* in, audio_buffer_t* out, int frames, bool accumulate) {
    const uint8_t inf = config_.inputCfg.format, of = config_.outputCfg.format;
    if (in->raw == out->raw && !accumulate) return;
    const size_t samples = static_cast<size_t>(frames) * channels_;
    if (!accumulate && inf == of) {
        std::memmove(out->raw, in->raw, samples * sampleBytes(inf));
        return;
    }
    const auto* src = static_cast<const uint8_t*>(in->raw);
    auto* dst = static_cast<uint8_t*>(out->raw);
    for (size_t i = 0; i < samples; ++i) store(dst, of, i, load(src, inf, i), accumulate);
}

int32_t EffectContext::process(audio_buffer_t* in, audio_buffer_t* out) {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (lock.owns_lock()) {
        if (disable_ != Disable::kNone) return -EINVAL;
        if (!enabled_) return -ENODATA;
    }
    if (!in && (config_.inputCfg.mask & EFFECT_CONFIG_BUFFER)) in = &config_.inputCfg.buffer;
    if (!out && (config_.outputCfg.mask & EFFECT_CONFIG_BUFFER)) out = &config_.outputCfg.buffer;
    if (!in || !out || !in->raw || !out->raw || in->frameCount != out->frameCount || in->frameCount == 0) return -EINVAL;

    const bool accumulate = config_.outputCfg.accessMode == EFFECT_BUFFER_ACCESS_ACCUMULATE;
    const int total = static_cast<int>(in->frameCount);
    if (!lock.owns_lock()) {
        passThrough(in, out, total, accumulate);
        return 0;
    }
    // Work through the buffer in engine-sized pieces (no allocation on the audio thread).
    for (int done = 0; done < total; done += Engine::kMaxBlock) {
        const int n = std::min(Engine::kMaxBlock, total - done);
        const size_t inOffset = static_cast<size_t>(done) * channels_ * sampleBytes(config_.inputCfg.format);
        const size_t outOffset = static_cast<size_t>(done) * channels_ * sampleBytes(config_.outputCfg.format);
        audio_buffer_t inPart{static_cast<size_t>(n), {static_cast<uint8_t*>(in->raw) + inOffset}};
        audio_buffer_t outPart{static_cast<size_t>(n), {static_cast<uint8_t*>(out->raw) + outOffset}};
        readInput(&inPart, n);
        if (abxOn_) {
            std::copy_n(left_.data(), n, refLeft_.data());
            std::copy_n(right_.data(), n, refRight_.data());
            engine_.process(left_.data(), right_.data(), n);
            reference_.process(refLeft_.data(), refRight_.data(), n);
            abx_.process(refLeft_.data(), refRight_.data(), reference_.latency(), left_.data(), right_.data(),
                         engine_.latency(), n);
        } else {
            engine_.process(left_.data(), right_.data(), n);
        }
        writeOutput(&inPart, &outPart, n, accumulate);
    }
    return 0;
}

}  // namespace rfx::android
