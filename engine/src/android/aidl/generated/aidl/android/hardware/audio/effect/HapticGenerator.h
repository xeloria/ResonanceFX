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
#include <aidl/android/hardware/audio/effect/HapticGenerator.h>
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
class HapticGenerator {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  enum class Tag : int32_t {
    vendor = 0,
    hapticScales = 1,
    vibratorInfo = 2,
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
        std::is_constructible_v<std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::HapticGenerator::Tag>, _Tp>
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
    std::variant<::aidl::android::hardware::audio::effect::VendorExtension, ::aidl::android::hardware::audio::effect::HapticGenerator::Tag> _value;
  };
  enum class VibratorScale : int32_t {
    MUTE = -100,
    VERY_LOW = -2,
    LOW = -1,
    NONE = 0,
    HIGH = 1,
    VERY_HIGH = 2,
  };

  class HapticScale {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t id = 0;
    ::aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale scale = ::aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::MUTE;
    float scaleFactor = -1.000000f;
    float adaptiveScaleFactor = -1.000000f;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const HapticScale& _rhs) const {
      return std::tie(id, scale, scaleFactor, adaptiveScaleFactor) == std::tie(_rhs.id, _rhs.scale, _rhs.scaleFactor, _rhs.adaptiveScaleFactor);
    }
    inline bool operator<(const HapticScale& _rhs) const {
      return std::tie(id, scale, scaleFactor, adaptiveScaleFactor) < std::tie(_rhs.id, _rhs.scale, _rhs.scaleFactor, _rhs.adaptiveScaleFactor);
    }
    inline bool operator!=(const HapticScale& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const HapticScale& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const HapticScale& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const HapticScale& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    static constexpr float UNDEFINED_SCALE_FACTOR = -1.000000f;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "HapticScale{";
      _aidl_os << "id: " << ::android::internal::ToString(id);
      _aidl_os << ", scale: " << ::android::internal::ToString(scale);
      _aidl_os << ", scaleFactor: " << ::android::internal::ToString(scaleFactor);
      _aidl_os << ", adaptiveScaleFactor: " << ::android::internal::ToString(adaptiveScaleFactor);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class VibratorInformation {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    float resonantFrequencyHz = 0.000000f;
    float qFactor = 0.000000f;
    float maxAmplitude = 0.000000f;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const VibratorInformation& _rhs) const {
      return std::tie(resonantFrequencyHz, qFactor, maxAmplitude) == std::tie(_rhs.resonantFrequencyHz, _rhs.qFactor, _rhs.maxAmplitude);
    }
    inline bool operator<(const VibratorInformation& _rhs) const {
      return std::tie(resonantFrequencyHz, qFactor, maxAmplitude) < std::tie(_rhs.resonantFrequencyHz, _rhs.qFactor, _rhs.maxAmplitude);
    }
    inline bool operator!=(const VibratorInformation& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const VibratorInformation& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const VibratorInformation& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const VibratorInformation& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "VibratorInformation{";
      _aidl_os << "resonantFrequencyHz: " << ::android::internal::ToString(resonantFrequencyHz);
      _aidl_os << ", qFactor: " << ::android::internal::ToString(qFactor);
      _aidl_os << ", maxAmplitude: " << ::android::internal::ToString(maxAmplitude);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  // Expose tag symbols for legacy code
  static const inline Tag vendor = Tag::vendor;
  static const inline Tag hapticScales = Tag::hapticScales;
  static const inline Tag vibratorInfo = Tag::vibratorInfo;

  template<typename _Tp>
  static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, HapticGenerator>;

  HapticGenerator() : _value(std::in_place_index<static_cast<size_t>(vendor)>, ::aidl::android::hardware::audio::effect::VendorExtension()) { }

  template <typename _Tp, typename = std::enable_if_t<
      _not_self<_Tp> &&
      std::is_constructible_v<std::variant<::aidl::android::hardware::audio::effect::VendorExtension, std::vector<::aidl::android::hardware::audio::effect::HapticGenerator::HapticScale>, ::aidl::android::hardware::audio::effect::HapticGenerator::VibratorInformation>, _Tp>
    >>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr HapticGenerator(_Tp&& _arg)
      : _value(std::forward<_Tp>(_arg)) {}

  template <size_t _Np, typename... _Tp>
  constexpr explicit HapticGenerator(std::in_place_index_t<_Np>, _Tp&&... _args)
      : _value(std::in_place_index<_Np>, std::forward<_Tp>(_args)...) {}

  template <Tag _tag, typename... _Tp>
  static HapticGenerator make(_Tp&&... _args) {
    return HapticGenerator(std::in_place_index<static_cast<size_t>(_tag)>, std::forward<_Tp>(_args)...);
  }

  template <Tag _tag, typename _Tp, typename... _Up>
  static HapticGenerator make(std::initializer_list<_Tp> _il, _Up&&... _args) {
    return HapticGenerator(std::in_place_index<static_cast<size_t>(_tag)>, std::move(_il), std::forward<_Up>(_args)...);
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

  inline bool operator==(const HapticGenerator& _rhs) const {
    return _value == _rhs._value;
  }
  inline bool operator<(const HapticGenerator& _rhs) const {
    return _value < _rhs._value;
  }
  inline bool operator!=(const HapticGenerator& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const HapticGenerator& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const HapticGenerator& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const HapticGenerator& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream os;
    os << "HapticGenerator{";
    switch (getTag()) {
    case vendor: os << "vendor: " << ::android::internal::ToString(get<vendor>()); break;
    case hapticScales: os << "hapticScales: " << ::android::internal::ToString(get<hapticScales>()); break;
    case vibratorInfo: os << "vibratorInfo: " << ::android::internal::ToString(get<vibratorInfo>()); break;
    }
    os << "}";
    return os.str();
  }
private:
  std::variant<::aidl::android::hardware::audio::effect::VendorExtension, std::vector<::aidl::android::hardware::audio::effect::HapticGenerator::HapticScale>, ::aidl::android::hardware::audio::effect::HapticGenerator::VibratorInformation> _value;
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
[[nodiscard]] static inline std::string toString(HapticGenerator::Id::Tag val) {
  switch(val) {
  case HapticGenerator::Id::Tag::vendorExtensionTag:
    return "vendorExtensionTag";
  case HapticGenerator::Id::Tag::commonTag:
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
constexpr inline std::array<aidl::android::hardware::audio::effect::HapticGenerator::Id::Tag, 2> enum_values<aidl::android::hardware::audio::effect::HapticGenerator::Id::Tag> = {
  aidl::android::hardware::audio::effect::HapticGenerator::Id::Tag::vendorExtensionTag,
  aidl::android::hardware::audio::effect::HapticGenerator::Id::Tag::commonTag,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(HapticGenerator::VibratorScale val) {
  switch(val) {
  case HapticGenerator::VibratorScale::MUTE:
    return "MUTE";
  case HapticGenerator::VibratorScale::VERY_LOW:
    return "VERY_LOW";
  case HapticGenerator::VibratorScale::LOW:
    return "LOW";
  case HapticGenerator::VibratorScale::NONE:
    return "NONE";
  case HapticGenerator::VibratorScale::HIGH:
    return "HIGH";
  case HapticGenerator::VibratorScale::VERY_HIGH:
    return "VERY_HIGH";
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
constexpr inline std::array<aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale, 6> enum_values<aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale> = {
  aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::MUTE,
  aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::VERY_LOW,
  aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::LOW,
  aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::NONE,
  aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::HIGH,
  aidl::android::hardware::audio::effect::HapticGenerator::VibratorScale::VERY_HIGH,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace hardware {
namespace audio {
namespace effect {
[[nodiscard]] static inline std::string toString(HapticGenerator::Tag val) {
  switch(val) {
  case HapticGenerator::Tag::vendor:
    return "vendor";
  case HapticGenerator::Tag::hapticScales:
    return "hapticScales";
  case HapticGenerator::Tag::vibratorInfo:
    return "vibratorInfo";
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
constexpr inline std::array<aidl::android::hardware::audio::effect::HapticGenerator::Tag, 3> enum_values<aidl::android::hardware::audio::effect::HapticGenerator::Tag> = {
  aidl::android::hardware::audio::effect::HapticGenerator::Tag::vendor,
  aidl::android::hardware::audio::effect::HapticGenerator::Tag::hapticScales,
  aidl::android::hardware::audio::effect::HapticGenerator::Tag::vibratorInfo,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
