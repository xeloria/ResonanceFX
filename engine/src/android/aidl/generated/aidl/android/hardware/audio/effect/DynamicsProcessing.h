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
#include <aidl/android/hardware/audio/effect/DynamicsProcessing.h>
#include <aidl/android/hardware/audio/effect/VendorExtension.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

#ifndef __BIONIC__
#define __assert2(a,b,c,d) ((void)0)
#endif

namespace aidl::android::hardware::audio::effect {
class VendorExtension;
}  // namespace aidl::android::hardware::audio::effect
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
class DynamicsProcessing {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  enum class Tag : int32_t {
    vendor = 0,
    engineArchitecture = 1,
    preEq = 2,
    postEq = 3,
    preEqBand = 4,
    postEqBand = 5,
    mbc = 6,
    mbcBand = 7,
    limiter = 8,
    inputGain = 9,
  };

  class Id {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    enum class Tag : int32_t {
      vendorExtensionTag = 0,
      commonTag = 1,
    };

    // Expose tag symbols for legacy code
    static const inline Tag vendorExtensionTag = Tag::vendorExtensionTag;
    static const inline Tag commonTag = Tag::commonTag;

    template<typename _Tp>
    static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, Id>;

    Id() : _value(std::in_place_index<static_cast<size_t>(vendorExtensionTag)>, ::aidl::android::hardware::audio::effect::VendorExtension()) { }

    template <typename _Tp, typename = std::enable_if_t<
        _not_self<_Tp> &&
        std::is_constructible_v<std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::DynamicsProcessing::Tag>, _Tp>
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
      }
      os << "}";
      return os.str();
    }
  private:
    std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::DynamicsProcessing::Tag> _value;
  };
  enum class ResolutionPreference : int8_t {
    FAVOR_FREQUENCY_RESOLUTION = 0,
    FAVOR_TIME_RESOLUTION = 1,
  };

  class StageEnablement {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    bool inUse = false;
    int32_t bandCount = 0;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const StageEnablement& _rhs) const {
      return std::tie(inUse, bandCount) == std::tie(_rhs.inUse, _rhs.bandCount);
    }
    inline bool operator<(const StageEnablement& _rhs) const {
      return std::tie(inUse, bandCount) < std::tie(_rhs.inUse, _rhs.bandCount);
    }
    inline bool operator!=(const StageEnablement& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const StageEnablement& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const StageEnablement& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const StageEnablement& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "StageEnablement{";
      _aidl_os << "inUse: " << ::android::internal::ToString(inUse);
      _aidl_os << ", bandCount: " << ::android::internal::ToString(bandCount);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class EngineArchitecture {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::aidl::android::hardware::audio::effect::DynamicsProcessing::ResolutionPreference resolutionPreference = ::aidl::android::hardware::audio::effect::DynamicsProcessing::ResolutionPreference::FAVOR_FREQUENCY_RESOLUTION;
    float preferredProcessingDurationMs = 0.000000f;
    ::aidl::android::hardware::audio::effect::DynamicsProcessing::StageEnablement preEqStage;
    ::aidl::android::hardware::audio::effect::DynamicsProcessing::StageEnablement postEqStage;
    ::aidl::android::hardware::audio::effect::DynamicsProcessing::StageEnablement mbcStage;
    bool limiterInUse = false;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const EngineArchitecture& _rhs) const {
      return std::tie(resolutionPreference, preferredProcessingDurationMs, preEqStage, postEqStage, mbcStage, limiterInUse) == std::tie(_rhs.resolutionPreference, _rhs.preferredProcessingDurationMs, _rhs.preEqStage, _rhs.postEqStage, _rhs.mbcStage, _rhs.limiterInUse);
    }
    inline bool operator<(const EngineArchitecture& _rhs) const {
      return std::tie(resolutionPreference, preferredProcessingDurationMs, preEqStage, postEqStage, mbcStage, limiterInUse) < std::tie(_rhs.resolutionPreference, _rhs.preferredProcessingDurationMs, _rhs.preEqStage, _rhs.postEqStage, _rhs.mbcStage, _rhs.limiterInUse);
    }
    inline bool operator!=(const EngineArchitecture& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const EngineArchitecture& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const EngineArchitecture& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const EngineArchitecture& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "EngineArchitecture{";
      _aidl_os << "resolutionPreference: " << ::android::internal::ToString(resolutionPreference);
      _aidl_os << ", preferredProcessingDurationMs: " << ::android::internal::ToString(preferredProcessingDurationMs);
      _aidl_os << ", preEqStage: " << ::android::internal::ToString(preEqStage);
      _aidl_os << ", postEqStage: " << ::android::internal::ToString(postEqStage);
      _aidl_os << ", mbcStage: " << ::android::internal::ToString(mbcStage);
      _aidl_os << ", limiterInUse: " << ::android::internal::ToString(limiterInUse);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class ChannelConfig {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t channel = 0;
    bool enable = false;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const ChannelConfig& _rhs) const {
      return std::tie(channel, enable) == std::tie(_rhs.channel, _rhs.enable);
    }
    inline bool operator<(const ChannelConfig& _rhs) const {
      return std::tie(channel, enable) < std::tie(_rhs.channel, _rhs.enable);
    }
    inline bool operator!=(const ChannelConfig& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const ChannelConfig& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const ChannelConfig& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const ChannelConfig& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "ChannelConfig{";
      _aidl_os << "channel: " << ::android::internal::ToString(channel);
      _aidl_os << ", enable: " << ::android::internal::ToString(enable);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class EqBandConfig {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t channel = 0;
    int32_t band = 0;
    bool enable = false;
    float cutoffFrequencyHz = 0.000000f;
    float gainDb = 0.000000f;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const EqBandConfig& _rhs) const {
      return std::tie(channel, band, enable, cutoffFrequencyHz, gainDb) == std::tie(_rhs.channel, _rhs.band, _rhs.enable, _rhs.cutoffFrequencyHz, _rhs.gainDb);
    }
    inline bool operator<(const EqBandConfig& _rhs) const {
      return std::tie(channel, band, enable, cutoffFrequencyHz, gainDb) < std::tie(_rhs.channel, _rhs.band, _rhs.enable, _rhs.cutoffFrequencyHz, _rhs.gainDb);
    }
    inline bool operator!=(const EqBandConfig& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const EqBandConfig& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const EqBandConfig& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const EqBandConfig& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "EqBandConfig{";
      _aidl_os << "channel: " << ::android::internal::ToString(channel);
      _aidl_os << ", band: " << ::android::internal::ToString(band);
      _aidl_os << ", enable: " << ::android::internal::ToString(enable);
      _aidl_os << ", cutoffFrequencyHz: " << ::android::internal::ToString(cutoffFrequencyHz);
      _aidl_os << ", gainDb: " << ::android::internal::ToString(gainDb);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class MbcBandConfig {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t channel = 0;
    int32_t band = 0;
    bool enable = false;
    float cutoffFrequencyHz = 0.000000f;
    float attackTimeMs = 0.000000f;
    float releaseTimeMs = 0.000000f;
    float ratio = 0.000000f;
    float thresholdDb = 0.000000f;
    float kneeWidthDb = 0.000000f;
    float noiseGateThresholdDb = 0.000000f;
    float expanderRatio = 0.000000f;
    float preGainDb = 0.000000f;
    float postGainDb = 0.000000f;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const MbcBandConfig& _rhs) const {
      return std::tie(channel, band, enable, cutoffFrequencyHz, attackTimeMs, releaseTimeMs, ratio, thresholdDb, kneeWidthDb, noiseGateThresholdDb, expanderRatio, preGainDb, postGainDb) == std::tie(_rhs.channel, _rhs.band, _rhs.enable, _rhs.cutoffFrequencyHz, _rhs.attackTimeMs, _rhs.releaseTimeMs, _rhs.ratio, _rhs.thresholdDb, _rhs.kneeWidthDb, _rhs.noiseGateThresholdDb, _rhs.expanderRatio, _rhs.preGainDb, _rhs.postGainDb);
    }
    inline bool operator<(const MbcBandConfig& _rhs) const {
      return std::tie(channel, band, enable, cutoffFrequencyHz, attackTimeMs, releaseTimeMs, ratio, thresholdDb, kneeWidthDb, noiseGateThresholdDb, expanderRatio, preGainDb, postGainDb) < std::tie(_rhs.channel, _rhs.band, _rhs.enable, _rhs.cutoffFrequencyHz, _rhs.attackTimeMs, _rhs.releaseTimeMs, _rhs.ratio, _rhs.thresholdDb, _rhs.kneeWidthDb, _rhs.noiseGateThresholdDb, _rhs.expanderRatio, _rhs.preGainDb, _rhs.postGainDb);
    }
    inline bool operator!=(const MbcBandConfig& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const MbcBandConfig& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const MbcBandConfig& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const MbcBandConfig& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "MbcBandConfig{";
      _aidl_os << "channel: " << ::android::internal::ToString(channel);
      _aidl_os << ", band: " << ::android::internal::ToString(band);
      _aidl_os << ", enable: " << ::android::internal::ToString(enable);
      _aidl_os << ", cutoffFrequencyHz: " << ::android::internal::ToString(cutoffFrequencyHz);
      _aidl_os << ", attackTimeMs: " << ::android::internal::ToString(attackTimeMs);
      _aidl_os << ", releaseTimeMs: " << ::android::internal::ToString(releaseTimeMs);
      _aidl_os << ", ratio: " << ::android::internal::ToString(ratio);
      _aidl_os << ", thresholdDb: " << ::android::internal::ToString(thresholdDb);
      _aidl_os << ", kneeWidthDb: " << ::android::internal::ToString(kneeWidthDb);
      _aidl_os << ", noiseGateThresholdDb: " << ::android::internal::ToString(noiseGateThresholdDb);
      _aidl_os << ", expanderRatio: " << ::android::internal::ToString(expanderRatio);
      _aidl_os << ", preGainDb: " << ::android::internal::ToString(preGainDb);
      _aidl_os << ", postGainDb: " << ::android::internal::ToString(postGainDb);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class LimiterConfig {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t channel = 0;
    bool enable = false;
    int32_t linkGroup = 0;
    float attackTimeMs = 0.000000f;
    float releaseTimeMs = 0.000000f;
    float ratio = 0.000000f;
    float thresholdDb = 0.000000f;
    float postGainDb = 0.000000f;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const LimiterConfig& _rhs) const {
      return std::tie(channel, enable, linkGroup, attackTimeMs, releaseTimeMs, ratio, thresholdDb, postGainDb) == std::tie(_rhs.channel, _rhs.enable, _rhs.linkGroup, _rhs.attackTimeMs, _rhs.releaseTimeMs, _rhs.ratio, _rhs.thresholdDb, _rhs.postGainDb);
    }
    inline bool operator<(const LimiterConfig& _rhs) const {
      return std::tie(channel, enable, linkGroup, attackTimeMs, releaseTimeMs, ratio, thresholdDb, postGainDb) < std::tie(_rhs.channel, _rhs.enable, _rhs.linkGroup, _rhs.attackTimeMs, _rhs.releaseTimeMs, _rhs.ratio, _rhs.thresholdDb, _rhs.postGainDb);
    }
    inline bool operator!=(const LimiterConfig& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const LimiterConfig& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const LimiterConfig& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const LimiterConfig& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "LimiterConfig{";
      _aidl_os << "channel: " << ::android::internal::ToString(channel);
      _aidl_os << ", enable: " << ::android::internal::ToString(enable);
      _aidl_os << ", linkGroup: " << ::android::internal::ToString(linkGroup);
      _aidl_os << ", attackTimeMs: " << ::android::internal::ToString(attackTimeMs);
      _aidl_os << ", releaseTimeMs: " << ::android::internal::ToString(releaseTimeMs);
      _aidl_os << ", ratio: " << ::android::internal::ToString(ratio);
      _aidl_os << ", thresholdDb: " << ::android::internal::ToString(thresholdDb);
      _aidl_os << ", postGainDb: " << ::android::internal::ToString(postGainDb);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class InputGain {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t channel = 0;
    float gainDb = 0.000000f;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const InputGain& _rhs) const {
      return std::tie(channel, gainDb) == std::tie(_rhs.channel, _rhs.gainDb);
    }
    inline bool operator<(const InputGain& _rhs) const {
      return std::tie(channel, gainDb) < std::tie(_rhs.channel, _rhs.gainDb);
    }
    inline bool operator!=(const InputGain& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const InputGain& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const InputGain& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const InputGain& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "InputGain{";
      _aidl_os << "channel: " << ::android::internal::ToString(channel);
      _aidl_os << ", gainDb: " << ::android::internal::ToString(gainDb);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  // Expose tag symbols for legacy code
  static const inline Tag vendor = Tag::vendor;
  static const inline Tag engineArchitecture = Tag::engineArchitecture;
  static const inline Tag preEq = Tag::preEq;
  static const inline Tag postEq = Tag::postEq;
  static const inline Tag preEqBand = Tag::preEqBand;
  static const inline Tag postEqBand = Tag::postEqBand;
  static const inline Tag mbc = Tag::mbc;
  static const inline Tag mbcBand = Tag::mbcBand;
  static const inline Tag limiter = Tag::limiter;
  static const inline Tag inputGain = Tag::inputGain;

  template<typename _Tp>
  static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, DynamicsProcessing>;

  DynamicsProcessing() : _value(std::in_place_index<static_cast<size_t>(vendor)>, ::aidl::android::hardware::audio::effect::VendorExtension()) { }

  template <typename _Tp, typename = std::enable_if_t<
      _not_self<_Tp> &&
      std::is_constructible_v<std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::DynamicsProcessing::EngineArchitecture, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::ChannelConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::ChannelConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::EqBandConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::EqBandConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::ChannelConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::MbcBandConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::LimiterConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::InputGain>>, _Tp>
    >>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DynamicsProcessing(_Tp&& _arg)
      : _value(std::forward<_Tp>(_arg)) {}

  template <size_t _Np, typename... _Tp>
  constexpr explicit DynamicsProcessing(std::in_place_index_t<_Np>, _Tp&&... _args)
      : _value(std::in_place_index<_Np>, std::forward<_Tp>(_args)...) {}

  template <Tag _tag, typename... _Tp>
  static DynamicsProcessing make(_Tp&&... _args) {
    return DynamicsProcessing(std::in_place_index<static_cast<size_t>(_tag)>, std::forward<_Tp>(_args)...);
  }

  template <Tag _tag, typename _Tp, typename... _Up>
  static DynamicsProcessing make(std::initializer_list<_Tp> _il, _Up&&... _args) {
    return DynamicsProcessing(std::in_place_index<static_cast<size_t>(_tag)>, std::move(_il), std::forward<_Up>(_args)...);
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

  inline bool operator==(const DynamicsProcessing& _rhs) const {
    return _value == _rhs._value;
  }
  inline bool operator<(const DynamicsProcessing& _rhs) const {
    return _value < _rhs._value;
  }
  inline bool operator!=(const DynamicsProcessing& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const DynamicsProcessing& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const DynamicsProcessing& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const DynamicsProcessing& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream os;
    os << "DynamicsProcessing{";
    switch (getTag()) {
    case vendor: os << "vendor: " << ::android::internal::ToString(get<vendor>()); break;
    case engineArchitecture: os << "engineArchitecture: " << ::android::internal::ToString(get<engineArchitecture>()); break;
    case preEq: os << "preEq: " << ::android::internal::ToString(get<preEq>()); break;
    case postEq: os << "postEq: " << ::android::internal::ToString(get<postEq>()); break;
    case preEqBand: os << "preEqBand: " << ::android::internal::ToString(get<preEqBand>()); break;
    case postEqBand: os << "postEqBand: " << ::android::internal::ToString(get<postEqBand>()); break;
    case mbc: os << "mbc: " << ::android::internal::ToString(get<mbc>()); break;
    case mbcBand: os << "mbcBand: " << ::android::internal::ToString(get<mbcBand>()); break;
    case limiter: os << "limiter: " << ::android::internal::ToString(get<limiter>()); break;
    case inputGain: os << "inputGain: " << ::android::internal::ToString(get<inputGain>()); break;
    }
    os << "}";
    return os.str();
  }
private:
  std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::DynamicsProcessing::EngineArchitecture, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::ChannelConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::ChannelConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::EqBandConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::EqBandConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::ChannelConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::MbcBandConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::LimiterConfig>, std::vector<::aidl::android::hardware::audio::effect::DynamicsProcessing::InputGain>> _value;
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
[[nodiscard]] static inline std::string toString(DynamicsProcessing::Id::Tag val) {
  switch(val) {
  case DynamicsProcessing::Id::Tag::vendorExtensionTag:
    return "vendorExtensionTag";
  case DynamicsProcessing::Id::Tag::commonTag:
    return "commonTag";
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
constexpr inline std::array<aidl::android::hardware::audio::effect::DynamicsProcessing::Id::Tag, 2> enum_values<aidl::android::hardware::audio::effect::DynamicsProcessing::Id::Tag> = {
  aidl::android::hardware::audio::effect::DynamicsProcessing::Id::Tag::vendorExtensionTag,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Id::Tag::commonTag,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(DynamicsProcessing::ResolutionPreference val) {
  switch(val) {
  case DynamicsProcessing::ResolutionPreference::FAVOR_FREQUENCY_RESOLUTION:
    return "FAVOR_FREQUENCY_RESOLUTION";
  case DynamicsProcessing::ResolutionPreference::FAVOR_TIME_RESOLUTION:
    return "FAVOR_TIME_RESOLUTION";
  default:
    return std::to_string(static_cast<int8_t>(val));
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
constexpr inline std::array<aidl::android::hardware::audio::effect::DynamicsProcessing::ResolutionPreference, 2> enum_values<aidl::android::hardware::audio::effect::DynamicsProcessing::ResolutionPreference> = {
  aidl::android::hardware::audio::effect::DynamicsProcessing::ResolutionPreference::FAVOR_FREQUENCY_RESOLUTION,
  aidl::android::hardware::audio::effect::DynamicsProcessing::ResolutionPreference::FAVOR_TIME_RESOLUTION,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(DynamicsProcessing::Tag val) {
  switch(val) {
  case DynamicsProcessing::Tag::vendor:
    return "vendor";
  case DynamicsProcessing::Tag::engineArchitecture:
    return "engineArchitecture";
  case DynamicsProcessing::Tag::preEq:
    return "preEq";
  case DynamicsProcessing::Tag::postEq:
    return "postEq";
  case DynamicsProcessing::Tag::preEqBand:
    return "preEqBand";
  case DynamicsProcessing::Tag::postEqBand:
    return "postEqBand";
  case DynamicsProcessing::Tag::mbc:
    return "mbc";
  case DynamicsProcessing::Tag::mbcBand:
    return "mbcBand";
  case DynamicsProcessing::Tag::limiter:
    return "limiter";
  case DynamicsProcessing::Tag::inputGain:
    return "inputGain";
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
constexpr inline std::array<aidl::android::hardware::audio::effect::DynamicsProcessing::Tag, 10> enum_values<aidl::android::hardware::audio::effect::DynamicsProcessing::Tag> = {
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::vendor,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::engineArchitecture,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::preEq,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::postEq,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::preEqBand,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::postEqBand,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::mbc,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::mbcBand,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::limiter,
  aidl::android::hardware::audio::effect::DynamicsProcessing::Tag::inputGain,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
