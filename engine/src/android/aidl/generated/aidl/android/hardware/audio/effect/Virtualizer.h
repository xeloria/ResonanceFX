/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:\\Users\\-\\AppData\\Local\\Android\\Sdk\\build-tools\\36.0.0\\aidl.exe --lang=ndk --structured --stability=vintf --min_sdk_version=31 -o C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aidl-gen -h C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aidl-gen -I C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3 -I C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwacommon -I C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwcommon -I C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwfmq -I C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon -I C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\AcousticEchoCanceler.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\AutomaticGainControlV1.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\AutomaticGainControlV2.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\BassBoost.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Capability.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\CommandId.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\DefaultExtension.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Descriptor.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Downmix.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\DynamicsProcessing.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\EnvironmentalReverb.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Equalizer.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Eraser.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Flags.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\HapticGenerator.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\IEffect.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\IFactory.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\LoudnessEnhancer.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\NoiseSuppression.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Parameter.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\PresetReverb.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Processing.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Range.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Spatializer.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\State.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\VendorExtension.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Virtualizer.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Visualizer.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\effect3\\android\\hardware\\audio\\effect\\Volume.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwacommon\\android\\hardware\\audio\\common\\AudioOffloadMetadata.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwacommon\\android\\hardware\\audio\\common\\PlaybackTrackMetadata.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwacommon\\android\\hardware\\audio\\common\\RecordTrackMetadata.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwacommon\\android\\hardware\\audio\\common\\SinkMetadata.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwacommon\\android\\hardware\\audio\\common\\SourceMetadata.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwcommon\\android\\hardware\\common\\Ashmem.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwcommon\\android\\hardware\\common\\MappableFile.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwcommon\\android\\hardware\\common\\NativeHandle.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwfmq\\android\\hardware\\common\\fmq\\GrantorDescriptor.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwfmq\\android\\hardware\\common\\fmq\\MQDescriptor.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwfmq\\android\\hardware\\common\\fmq\\SynchronizedReadWrite.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\hwfmq\\android\\hardware\\common\\fmq\\UnsynchronizedWrite.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioAttributes.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioChannelLayout.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioConfig.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioConfigBase.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioContentType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioDevice.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioDeviceAddress.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioDeviceDescription.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioDeviceType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioDualMonoMode.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioEncapsulationMetadataType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioEncapsulationMode.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioEncapsulationType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioFlag.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioFormatDescription.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioFormatType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioGain.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioGainConfig.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioGainMode.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalAttributesGroup.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapConfiguration.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapCriterion.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapCriterionType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapCriterionV2.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapDomain.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapParameter.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalCapRule.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalEngineConfig.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalProductStrategy.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalVolumeCurve.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioHalVolumeGroup.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioInputFlags.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioIoFlags.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioLatencyMode.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioMMapPolicy.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioMMapPolicyInfo.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioMMapPolicyType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioMode.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioOffloadInfo.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioOutputFlags.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPlaybackRate.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPolicyForceUse.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPort.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPortConfig.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPortDeviceExt.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPortExt.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPortMixExt.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioPortMixExtUseCase.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioProductStrategyType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioProfile.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioSource.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioStandard.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioStreamType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioUsage.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioUuid.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\AudioVolumeGroupChangeEvent.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Boolean.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Byte.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Double.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\ExtraAudioDescriptor.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Float.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\HeadTracking.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Int.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Long.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\MicrophoneDynamicInfo.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\MicrophoneInfo.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\PcmType.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Spatialization.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\macommon\\android\\media\\audio\\common\\Void.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\Capability.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\Classification.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\ClassificationConfig.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\ClassificationMetadata.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\ClassificationMetadataList.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\ClassifierCapability.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\Configuration.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\IEraserCallback.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\Mode.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\RemixerCapability.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\SeparatorCapability.aidl C:\\Users\\-\\opencode\\Vipre\\ResonanceFX\\build\\aosp-src\\eraser\\android\\media\\audio\\eraser\\SoundClassification.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <android/binder_enums.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <aidl/android/hardware/audio/effect/VendorExtension.h>
#include <aidl/android/hardware/audio/effect/Virtualizer.h>
#include <aidl/android/media/audio/common/AudioChannelLayout.h>
#include <aidl/android/media/audio/common/AudioDeviceDescription.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

#ifndef __BIONIC__
#define __assert2(a,b,c,d) ((void)0)
#endif

namespace aidl::android::hardware::audio::effect {
class VendorExtension;
}  // namespace aidl::android::hardware::audio::effect
namespace aidl::android::media::audio::common {
class AudioDeviceDescription;
}  // namespace aidl::android::media::audio::common
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
class Virtualizer {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  enum class Tag : int32_t {
    vendor = 0,
    strengthPm = 1,
    speakerAngles = 2,
    device = 3,
  };

  class SpeakerAnglesPayload {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::aidl::android::media::audio::common::AudioChannelLayout layout;
    ::aidl::android::media::audio::common::AudioDeviceDescription device;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const SpeakerAnglesPayload& _rhs) const {
      return std::tie(layout, device) == std::tie(_rhs.layout, _rhs.device);
    }
    inline bool operator<(const SpeakerAnglesPayload& _rhs) const {
      return std::tie(layout, device) < std::tie(_rhs.layout, _rhs.device);
    }
    inline bool operator!=(const SpeakerAnglesPayload& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const SpeakerAnglesPayload& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const SpeakerAnglesPayload& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const SpeakerAnglesPayload& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "SpeakerAnglesPayload{";
      _aidl_os << "layout: " << ::android::internal::ToString(layout);
      _aidl_os << ", device: " << ::android::internal::ToString(device);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class Id {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    enum class Tag : int32_t {
      vendorExtensionTag = 0,
      commonTag = 1,
      speakerAnglesPayload = 2,
    };

    // Expose tag symbols for legacy code
    static const inline Tag vendorExtensionTag = Tag::vendorExtensionTag;
    static const inline Tag commonTag = Tag::commonTag;
    static const inline Tag speakerAnglesPayload = Tag::speakerAnglesPayload;

    template<typename _Tp>
    static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, Id>;

    Id() : _value(std::in_place_index<static_cast<size_t>(vendorExtensionTag)>, ::aidl::android::hardware::audio::effect::VendorExtension()) { }

    template <typename _Tp, typename = std::enable_if_t<
        _not_self<_Tp> &&
        std::is_constructible_v<std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::Virtualizer::Tag, ::aidl::android::hardware::audio::effect::Virtualizer::SpeakerAnglesPayload>, _Tp>
      >>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Id(_Tp&& _arg)
        : _value(std::forward<_Tp>(_arg)) {}

    template <size_t _Np, typename... _Tp>
    constexpr explicit Id(std::in_place_index_t<_Np>, _Tp&&... _args)
        : _value(std::in_place_index<_Np>, std::forward<_Tp>(_args)...) {}

    template <Tag _tag, typename... _Tp>
    static Id make(_Tp&&... _args) {
      return Id(std::in_place_index<static_cast<size_t>(_tag)>, std::forward<_Tp>(_args)...);
    }

    template <Tag _tag, typename _Tp, typename... _Up>
    static Id make(std::initializer_list<_Tp> _il, _Up&&... _args) {
      return Id(std::in_place_index<static_cast<size_t>(_tag)>, std::move(_il), std::forward<_Up>(_args)...);
    }

    Tag getTag() const {
      return static_cast<Tag>(_value.index());
    }

    template <Tag _tag>
    const auto& get() const {
      if (getTag() != _tag) { __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "bad access: a wrong tag"); }
      return std::get<static_cast<size_t>(_tag)>(_value);
    }

    template <Tag _tag>
    auto& get() {
      if (getTag() != _tag) { __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "bad access: a wrong tag"); }
      return std::get<static_cast<size_t>(_tag)>(_value);
    }

    template <Tag _tag, typename... _Tp>
    void set(_Tp&&... _args) {
      _value.emplace<static_cast<size_t>(_tag)>(std::forward<_Tp>(_args)...);
    }

    binder_status_t readFromParcel(const AParcel* _parcel);
    binder_status_t writeToParcel(AParcel* _parcel) const;

    inline bool operator==(const Id& _rhs) const {
      return _value == _rhs._value;
    }
    inline bool operator<(const Id& _rhs) const {
      return _value < _rhs._value;
    }
    inline bool operator!=(const Id& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const Id& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const Id& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const Id& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream os;
      os << "Id{";
      switch (getTag()) {
      case vendorExtensionTag: os << "vendorExtensionTag: " << ::android::internal::ToString(get<vendorExtensionTag>()); break;
      case commonTag: os << "commonTag: " << ::android::internal::ToString(get<commonTag>()); break;
      case speakerAnglesPayload: os << "speakerAnglesPayload: " << ::android::internal::ToString(get<speakerAnglesPayload>()); break;
      }
      os << "}";
      return os.str();
    }
  private:
    std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::Virtualizer::Tag, ::aidl::android::hardware::audio::effect::Virtualizer::SpeakerAnglesPayload> _value;
  };
  class ChannelAngle {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t channel = 0;
    int32_t azimuthDegree = 0;
    int32_t elevationDegree = 0;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const ChannelAngle& _rhs) const {
      return std::tie(channel, azimuthDegree, elevationDegree) == std::tie(_rhs.channel, _rhs.azimuthDegree, _rhs.elevationDegree);
    }
    inline bool operator<(const ChannelAngle& _rhs) const {
      return std::tie(channel, azimuthDegree, elevationDegree) < std::tie(_rhs.channel, _rhs.azimuthDegree, _rhs.elevationDegree);
    }
    inline bool operator!=(const ChannelAngle& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const ChannelAngle& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const ChannelAngle& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const ChannelAngle& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "ChannelAngle{";
      _aidl_os << "channel: " << ::android::internal::ToString(channel);
      _aidl_os << ", azimuthDegree: " << ::android::internal::ToString(azimuthDegree);
      _aidl_os << ", elevationDegree: " << ::android::internal::ToString(elevationDegree);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  // Expose tag symbols for legacy code
  static const inline Tag vendor = Tag::vendor;
  static const inline Tag strengthPm = Tag::strengthPm;
  static const inline Tag speakerAngles = Tag::speakerAngles;
  static const inline Tag device = Tag::device;

  template<typename _Tp>
  static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, Virtualizer>;

  Virtualizer() : _value(std::in_place_index<static_cast<size_t>(vendor)>, ::aidl::android::hardware::audio::effect::VendorExtension()) { }

  template <typename _Tp, typename = std::enable_if_t<
      _not_self<_Tp> &&
      std::is_constructible_v<std::variant<::aidl::android::hardware::audio::effect::VendorExtension, int32_t, std::vector<::aidl::android::hardware::audio::effect::Virtualizer::ChannelAngle>, ::aidl::android::media::audio::common::AudioDeviceDescription>, _Tp>
    >>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Virtualizer(_Tp&& _arg)
      : _value(std::forward<_Tp>(_arg)) {}

  template <size_t _Np, typename... _Tp>
  constexpr explicit Virtualizer(std::in_place_index_t<_Np>, _Tp&&... _args)
      : _value(std::in_place_index<_Np>, std::forward<_Tp>(_args)...) {}

  template <Tag _tag, typename... _Tp>
  static Virtualizer make(_Tp&&... _args) {
    return Virtualizer(std::in_place_index<static_cast<size_t>(_tag)>, std::forward<_Tp>(_args)...);
  }

  template <Tag _tag, typename _Tp, typename... _Up>
  static Virtualizer make(std::initializer_list<_Tp> _il, _Up&&... _args) {
    return Virtualizer(std::in_place_index<static_cast<size_t>(_tag)>, std::move(_il), std::forward<_Up>(_args)...);
  }

  Tag getTag() const {
    return static_cast<Tag>(_value.index());
  }

  template <Tag _tag>
  const auto& get() const {
    if (getTag() != _tag) { __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "bad access: a wrong tag"); }
    return std::get<static_cast<size_t>(_tag)>(_value);
  }

  template <Tag _tag>
  auto& get() {
    if (getTag() != _tag) { __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "bad access: a wrong tag"); }
    return std::get<static_cast<size_t>(_tag)>(_value);
  }

  template <Tag _tag, typename... _Tp>
  void set(_Tp&&... _args) {
    _value.emplace<static_cast<size_t>(_tag)>(std::forward<_Tp>(_args)...);
  }

  binder_status_t readFromParcel(const AParcel* _parcel);
  binder_status_t writeToParcel(AParcel* _parcel) const;

  inline bool operator==(const Virtualizer& _rhs) const {
    return _value == _rhs._value;
  }
  inline bool operator<(const Virtualizer& _rhs) const {
    return _value < _rhs._value;
  }
  inline bool operator!=(const Virtualizer& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const Virtualizer& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const Virtualizer& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const Virtualizer& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream os;
    os << "Virtualizer{";
    switch (getTag()) {
    case vendor: os << "vendor: " << ::android::internal::ToString(get<vendor>()); break;
    case strengthPm: os << "strengthPm: " << ::android::internal::ToString(get<strengthPm>()); break;
    case speakerAngles: os << "speakerAngles: " << ::android::internal::ToString(get<speakerAngles>()); break;
    case device: os << "device: " << ::android::internal::ToString(get<device>()); break;
    }
    os << "}";
    return os.str();
  }
private:
  std::variant<::aidl::android::hardware::audio::effect::VendorExtension, int32_t, std::vector<::aidl::android::hardware::audio::effect::Virtualizer::ChannelAngle>, ::aidl::android::media::audio::common::AudioDeviceDescription> _value;
};
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(Virtualizer::Id::Tag val) {
  switch(val) {
  case Virtualizer::Id::Tag::vendorExtensionTag:
    return "vendorExtensionTag";
  case Virtualizer::Id::Tag::commonTag:
    return "commonTag";
  case Virtualizer::Id::Tag::speakerAnglesPayload:
    return "speakerAnglesPayload";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::audio::effect::Virtualizer::Id::Tag, 3> enum_values<aidl::android::hardware::audio::effect::Virtualizer::Id::Tag> = {
  aidl::android::hardware::audio::effect::Virtualizer::Id::Tag::vendorExtensionTag,
  aidl::android::hardware::audio::effect::Virtualizer::Id::Tag::commonTag,
  aidl::android::hardware::audio::effect::Virtualizer::Id::Tag::speakerAnglesPayload,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(Virtualizer::Tag val) {
  switch(val) {
  case Virtualizer::Tag::vendor:
    return "vendor";
  case Virtualizer::Tag::strengthPm:
    return "strengthPm";
  case Virtualizer::Tag::speakerAngles:
    return "speakerAngles";
  case Virtualizer::Tag::device:
    return "device";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::audio::effect::Virtualizer::Tag, 4> enum_values<aidl::android::hardware::audio::effect::Virtualizer::Tag> = {
  aidl::android::hardware::audio::effect::Virtualizer::Tag::vendor,
  aidl::android::hardware::audio::effect::Virtualizer::Tag::strengthPm,
  aidl::android::hardware::audio::effect::Virtualizer::Tag::speakerAngles,
  aidl::android::hardware::audio::effect::Virtualizer::Tag::device,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
