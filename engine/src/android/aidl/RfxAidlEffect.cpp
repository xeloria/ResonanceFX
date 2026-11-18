// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
//
// AIDL effect front-end for ResonanceFX (Android 16+ effect HALs).
//
// The DSP, the parameter protocol and the stream conversion stay in the legacy
// rfx::android::EffectContext. This file only adapts the AIDL contract to it:
//   * Parameter::Common -> effect_config_t (float32, same rate, same channel layout)
//   * DefaultExtension.bytes (a full legacy effect_param_t) -> EFFECT_CMD_SET/GET_PARAM
//   * the float FMQ block -> audio_buffer_t
//
// The IEffect state machine, FMQ queues and worker thread come from AOSP's EffectImpl
// (Apache-2.0, vendored in third_party/aosp).

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#include <aidl/android/hardware/audio/effect/DefaultExtension.h>
#include <aidl/android/hardware/audio/effect/Descriptor.h>
#include <aidl/android/hardware/audio/effect/Flags.h>
#include <aidl/android/hardware/audio/effect/VendorExtension.h>
#include <aidl/android/media/audio/common/AudioChannelLayout.h>
#include <aidl/android/media/audio/common/AudioUuid.h>
#include <android/binder_auto_utils.h>

#include "effect-impl/EffectImpl.h"

#include "Utils.h"

#include "../EffectContext.h"
#include "../android_effect_abi.h"

namespace rfx::aidl {

using ::aidl::android::hardware::audio::effect::CommandId;
using ::aidl::android::hardware::audio::effect::DefaultExtension;
using ::aidl::android::hardware::audio::effect::Descriptor;
using ::aidl::android::hardware::audio::effect::EffectContext;
using ::aidl::android::hardware::audio::effect::EffectImpl;
using ::aidl::android::hardware::audio::effect::Flags;
using ::aidl::android::hardware::audio::effect::IEffect;
using ::aidl::android::hardware::audio::effect::Parameter;
using ::aidl::android::hardware::audio::effect::RetCode;
using ::aidl::android::hardware::audio::effect::VendorExtension;
using ::aidl::android::media::audio::common::AudioChannelLayout;
using ::aidl::android::media::audio::common::AudioUuid;
using ::aidl::android::media::audio::common::PcmType;

namespace {

AudioUuid makeUuid(int32_t timeLow, int32_t timeMid, int32_t timeHiAndVersion, int32_t clockSeq,
                   std::vector<uint8_t> node) {
    AudioUuid uuid;
    uuid.timeLow = timeLow;
    uuid.timeMid = timeMid;
    uuid.timeHiAndVersion = timeHiAndVersion;
    uuid.clockSeq = clockSeq;
    uuid.node = std::move(node);
    return uuid;
}

const AudioUuid& typeUuid() {
    static const AudioUuid uuid =
            makeUuid(0xec7178ec, 0xe5e1, 0x4432, 0xa3f4, {0x46, 0x57, 0xe6, 0x79, 0x52, 0x10});
    return uuid;
}

const AudioUuid& implementationUuid() {
    static const AudioUuid uuid =
            makeUuid(0x4daa8eaa, 0x0842, 0x4a8d, 0x82e9, {0x6e, 0xee, 0x58, 0x27, 0xfb, 0xe4});
    return uuid;
}

/// Copies AIDL bytes into a 4-byte aligned buffer (effect_param_t is int-aligned).
std::vector<uint32_t> alignedCopy(const std::vector<uint8_t>& bytes) {
    std::vector<uint32_t> buffer((bytes.size() + sizeof(uint32_t) - 1) / sizeof(uint32_t));
    if (!bytes.empty()) std::memcpy(buffer.data(), bytes.data(), bytes.size());
    return buffer;
}

int32_t legacyCommand(rfx::android::EffectContext& context, uint32_t code, void* data,
                      uint32_t size, void* reply, uint32_t* replySize) {
    return context.command(code, size, data, replySize, reply);
}

}  // namespace

Descriptor descriptor() {
    Descriptor d;
    d.common.id.type = typeUuid();
    d.common.id.uuid = implementationUuid();
    d.common.id.proxy = std::nullopt;
    d.common.flags.type = Flags::Type::INSERT;
    d.common.flags.insert = Flags::Insert::LAST;
    d.common.cpuLoad = 8;
    d.common.memoryUsage = 1;
    d.common.name = "ResonanceFX";
    d.common.implementor = "Xeloria";
    return d;
}

/// The AIDL context owns the legacy engine context and keeps it configured.
class RfxAidlContext final : public EffectContext {
  public:
    explicit RfxAidlContext(const Parameter::Common& common) : EffectContext(1, common) {
        ready_ = configureLegacy(common);
    }

    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] size_t channels() const { return channels_; }
    rfx::android::EffectContext& legacy() { return legacy_; }

    RetCode setCommon(const Parameter::Common& common) override {
        const RetCode ret = EffectContext::setCommon(common);
        if (ret != RetCode::SUCCESS) return ret;
        return configureLegacy(common) ? RetCode::SUCCESS : RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    RetCode enable() override { return lifecycle(EFFECT_CMD_ENABLE); }
    RetCode disable() override { return lifecycle(EFFECT_CMD_DISABLE); }
    RetCode reset() override { return lifecycle(EFFECT_CMD_RESET); }

  private:
    RetCode lifecycle(uint32_t code) {
        int32_t status = -EINVAL;
        uint32_t replySize = sizeof(status);
        const int32_t ret = legacyCommand(legacy_, code, nullptr, 0, &status, &replySize);
        if (ret != 0 || status != 0) {
            LOG(ERROR) << __func__ << " legacy command " << code << " failed: " << ret;
            return RetCode::ERROR_EFFECT_LIB_ERROR;
        }
        return RetCode::SUCCESS;
    }

    bool configureLegacy(const Parameter::Common& common) {
        using ::aidl::android::hardware::audio::common::getChannelCount;

        const auto& input = common.input.base;
        const auto& output = common.output.base;
        if (input.format.pcm != PcmType::FLOAT_32_BIT ||
            output.format.pcm != PcmType::FLOAT_32_BIT) {
            LOG(ERROR) << __func__ << " AIDL streams must be float32";
            return false;
        }
        if (input.sampleRate != output.sampleRate || input.sampleRate <= 0) {
            LOG(ERROR) << __func__ << " input and output rates differ";
            return false;
        }
        const size_t inChannels = getChannelCount(input.channelMask);
        const size_t outChannels = getChannelCount(output.channelMask);
        if (inChannels == 0 || inChannels != outChannels) {
            LOG(ERROR) << __func__ << " unsupported channel layout";
            return false;
        }
        auto layoutMask = [](const ::aidl::android::media::audio::common::AudioChannelLayout& layout) {
            return layout.getTag() == AudioChannelLayout::layoutMask
                           ? layout.get<AudioChannelLayout::layoutMask>()
                           : 0;
        };
        const int32_t inMask = layoutMask(input.channelMask);
        const int32_t outMask = layoutMask(output.channelMask);
        if (inMask == 0 || outMask == 0) {
            LOG(ERROR) << __func__ << " channel layout has no mask";
            return false;
        }

        effect_config_t config{};
        const uint32_t mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS |
                              EFFECT_CONFIG_FORMAT | EFFECT_CONFIG_ACC_MODE;
        config.inputCfg.mask = mask;
        config.outputCfg.mask = mask;
        config.inputCfg.samplingRate = static_cast<uint32_t>(input.sampleRate);
        config.outputCfg.samplingRate = static_cast<uint32_t>(output.sampleRate);
        config.inputCfg.channels = static_cast<uint32_t>(inMask);
        config.outputCfg.channels = static_cast<uint32_t>(outMask);
        config.inputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
        config.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
        config.inputCfg.accessMode = EFFECT_BUFFER_ACCESS_READ;
        config.outputCfg.accessMode = EFFECT_BUFFER_ACCESS_WRITE;

        int32_t status = -EINVAL;
        uint32_t replySize = sizeof(status);
        const int32_t ret = legacyCommand(legacy_, EFFECT_CMD_SET_CONFIG, &config,
                                          sizeof(config), &status, &replySize);
        if (ret != 0 || status != 0) {
            LOG(ERROR) << __func__ << " set config failed: " << ret << "/" << status;
            return false;
        }
        channels_ = inChannels;
        return true;
    }

    rfx::android::EffectContext legacy_;
    size_t channels_ = 0;
    bool ready_ = false;
};

class RfxAidlEffect final : public EffectImpl {
  public:
    ~RfxAidlEffect() override { cleanUp(); }

    ndk::ScopedAStatus getDescriptor(Descriptor* descriptor) override {
        if (descriptor == nullptr) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *descriptor = rfx::aidl::descriptor();
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus setParameterSpecific(const Parameter::Specific& specific)
            REQUIRES(mImplMutex) override {
        if (specific.getTag() != Parameter::Specific::vendorEffect || !context_) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "notVendorEffect");
        }
        const auto& vendor = specific.get<Parameter::Specific::vendorEffect>();
        std::optional<DefaultExtension> extension;
        if (vendor.extension.getParcelable(&extension) != STATUS_OK || !extension.has_value()) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "badExtension");
        }
        return setPacket(extension->bytes);
    }

    ndk::ScopedAStatus getParameterSpecific(const Parameter::Id& id,
                                            Parameter::Specific* specific) REQUIRES(mImplMutex) override {
        if (specific == nullptr || id.getTag() != Parameter::Id::vendorEffectTag || !context_) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "notVendorEffectId");
        }
        const auto& vendorId = id.get<Parameter::Id::vendorEffectTag>();
        std::optional<DefaultExtension> idExtension;
        if (vendorId.extension.getParcelable(&idExtension) != STATUS_OK ||
            !idExtension.has_value()) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "badIdExtension");
        }

        std::vector<uint32_t> request = alignedCopy(idExtension->bytes);
        // Large enough for the biggest parameter payload (32 parametric bands, 31 gains,
        // 512-point DDC tables), with the legacy header on top.
        constexpr size_t kReplyCapacity = sizeof(effect_param_t) + 16384;
        std::vector<uint32_t> reply((kReplyCapacity + sizeof(uint32_t) - 1) / sizeof(uint32_t));
        uint32_t replySize = static_cast<uint32_t>(reply.size() * sizeof(uint32_t));
        const int32_t ret = legacyCommand(context_->legacy(), EFFECT_CMD_GET_PARAM, request.data(),
                                          static_cast<uint32_t>(idExtension->bytes.size()),
                                          reply.data(), &replySize);
        if (ret != 0) {
            LOG(ERROR) << __func__ << " get parameter failed: " << ret;
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "legacyGetParamFailed");
        }

        DefaultExtension value;
        const auto* bytes = reinterpret_cast<const uint8_t*>(reply.data());
        value.bytes.assign(bytes, bytes + replySize);
        VendorExtension vendor;
        if (vendor.extension.setParcelable(value) != STATUS_OK) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "setParcelableFailed");
        }
        specific->set<Parameter::Specific::vendorEffect>(vendor);
        return ndk::ScopedAStatus::ok();
    }

    std::shared_ptr<EffectContext> createContext(const Parameter::Common& common)
            REQUIRES(mImplMutex) override {
        if (context_) return context_;
        auto candidate = std::make_shared<RfxAidlContext>(common);
        if (!candidate->ready()) return nullptr;
        context_ = std::move(candidate);
        return context_;
    }

    RetCode releaseContext() REQUIRES(mImplMutex) override {
        context_.reset();
        return RetCode::SUCCESS;
    }

    std::string getEffectName() override { return "ResonanceFX"; }

    IEffect::Status effectProcessImpl(float* input, float* output, int samples)
            REQUIRES(mImplMutex) override {
        if (!context_ || input == nullptr || output == nullptr || samples <= 0) {
            return {EX_ILLEGAL_ARGUMENT, 0, 0};
        }
        const size_t channels = context_->channels();
        if (channels == 0 || static_cast<size_t>(samples) % channels != 0) {
            return {EX_ILLEGAL_ARGUMENT, 0, 0};
        }
        const size_t frames = static_cast<size_t>(samples) / channels;
        audio_buffer_t inBuffer{};
        audio_buffer_t outBuffer{};
        inBuffer.frameCount = frames;
        inBuffer.f32 = input;
        outBuffer.frameCount = frames;
        outBuffer.f32 = output;

        const int32_t ret = context_->legacy().process(&inBuffer, &outBuffer);
        if (ret != 0) {
            // Effects off, stream rejected or a command holds the lock: pass audio through.
            std::memmove(output, input, sizeof(float) * static_cast<size_t>(samples));
        }
        return {STATUS_OK, samples, samples};
    }

  private:
    ndk::ScopedAStatus setPacket(const std::vector<uint8_t>& bytes) {
        if (bytes.size() < sizeof(effect_param_t)) {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "shortPacket");
        }
        std::vector<uint32_t> packet = alignedCopy(bytes);
        int32_t status = -EINVAL;
        uint32_t replySize = sizeof(status);
        const int32_t ret = legacyCommand(context_->legacy(), EFFECT_CMD_SET_PARAM, packet.data(),
                                          static_cast<uint32_t>(bytes.size()), &status, &replySize);
        if (ret != 0 || status != 0) {
            LOG(ERROR) << __func__ << " set parameter failed: " << ret << "/" << status;
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "legacySetParamFailed");
        }
        return ndk::ScopedAStatus::ok();
    }

    std::shared_ptr<RfxAidlContext> context_ GUARDED_BY(mImplMutex);
};

}  // namespace rfx::aidl

using ::aidl::android::hardware::audio::effect::IEffect;
using ::aidl::android::media::audio::common::AudioUuid;

extern "C" __attribute__((visibility("default"))) binder_exception_t createEffect(
        const AudioUuid* implementationUuid, std::shared_ptr<IEffect>* instance) {
    if (implementationUuid == nullptr || instance == nullptr ||
        *implementationUuid != rfx::aidl::implementationUuid()) {
        return EX_ILLEGAL_ARGUMENT;
    }
    *instance = ndk::SharedRefBase::make<rfx::aidl::RfxAidlEffect>();
    return EX_NONE;
}

extern "C" __attribute__((visibility("default"))) binder_exception_t queryEffect(
        const AudioUuid* implementationUuid,
        aidl::android::hardware::audio::effect::Descriptor* descriptor) {
    if (implementationUuid == nullptr || descriptor == nullptr ||
        *implementationUuid != rfx::aidl::implementationUuid()) {
        return EX_ILLEGAL_ARGUMENT;
    }
    *descriptor = rfx::aidl::descriptor();
    return EX_NONE;
}


