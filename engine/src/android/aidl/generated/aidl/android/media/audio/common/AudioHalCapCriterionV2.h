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
#include <aidl/android/media/audio/common/AudioDeviceAddress.h>
#include <aidl/android/media/audio/common/AudioDeviceDescription.h>
#include <aidl/android/media/audio/common/AudioHalCapCriterionV2.h>
#include <aidl/android/media/audio/common/AudioMode.h>
#include <aidl/android/media/audio/common/AudioPolicyForceUse.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

#ifndef __BIONIC__
#define __assert2(a,b,c,d) ((void)0)
#endif

namespace aidl::android::media::audio::common {
class AudioDeviceDescription;
}  // namespace aidl::android::media::audio::common
namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace common {
class AudioHalCapCriterionV2 {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  enum class LogicalDisjunction : int8_t {
    EXCLUSIVE = 0,
    INCLUSIVE = 1,
  };

  class ForceConfigForUse {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::vector<::aidl::android::media::audio::common::AudioPolicyForceUse> values;
    ::aidl::android::media::audio::common::AudioPolicyForceUse defaultValue;
    ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction logic = ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction::EXCLUSIVE;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const ForceConfigForUse& _rhs) const {
      return std::tie(values, defaultValue, logic) == std::tie(_rhs.values, _rhs.defaultValue, _rhs.logic);
    }
    inline bool operator<(const ForceConfigForUse& _rhs) const {
      return std::tie(values, defaultValue, logic) < std::tie(_rhs.values, _rhs.defaultValue, _rhs.logic);
    }
    inline bool operator!=(const ForceConfigForUse& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const ForceConfigForUse& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const ForceConfigForUse& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const ForceConfigForUse& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "ForceConfigForUse{";
      _aidl_os << "values: " << ::android::internal::ToString(values);
      _aidl_os << ", defaultValue: " << ::android::internal::ToString(defaultValue);
      _aidl_os << ", logic: " << ::android::internal::ToString(logic);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class TelephonyMode {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::vector<::aidl::android::media::audio::common::AudioMode> values;
    ::aidl::android::media::audio::common::AudioMode defaultValue = ::aidl::android::media::audio::common::AudioMode::NORMAL;
    ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction logic = ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction::EXCLUSIVE;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const TelephonyMode& _rhs) const {
      return std::tie(values, defaultValue, logic) == std::tie(_rhs.values, _rhs.defaultValue, _rhs.logic);
    }
    inline bool operator<(const TelephonyMode& _rhs) const {
      return std::tie(values, defaultValue, logic) < std::tie(_rhs.values, _rhs.defaultValue, _rhs.logic);
    }
    inline bool operator!=(const TelephonyMode& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const TelephonyMode& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const TelephonyMode& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const TelephonyMode& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "TelephonyMode{";
      _aidl_os << "values: " << ::android::internal::ToString(values);
      _aidl_os << ", defaultValue: " << ::android::internal::ToString(defaultValue);
      _aidl_os << ", logic: " << ::android::internal::ToString(logic);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class AvailableDevices {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::vector<::aidl::android::media::audio::common::AudioDeviceDescription> values;
    ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction logic = ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction::INCLUSIVE;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const AvailableDevices& _rhs) const {
      return std::tie(values, logic) == std::tie(_rhs.values, _rhs.logic);
    }
    inline bool operator<(const AvailableDevices& _rhs) const {
      return std::tie(values, logic) < std::tie(_rhs.values, _rhs.logic);
    }
    inline bool operator!=(const AvailableDevices& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const AvailableDevices& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const AvailableDevices& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const AvailableDevices& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "AvailableDevices{";
      _aidl_os << "values: " << ::android::internal::ToString(values);
      _aidl_os << ", logic: " << ::android::internal::ToString(logic);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class AvailableDevicesAddresses {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::vector<::aidl::android::media::audio::common::AudioDeviceAddress> values;
    ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction logic = ::aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction::INCLUSIVE;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const AvailableDevicesAddresses& _rhs) const {
      return std::tie(values, logic) == std::tie(_rhs.values, _rhs.logic);
    }
    inline bool operator<(const AvailableDevicesAddresses& _rhs) const {
      return std::tie(values, logic) < std::tie(_rhs.values, _rhs.logic);
    }
    inline bool operator!=(const AvailableDevicesAddresses& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const AvailableDevicesAddresses& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const AvailableDevicesAddresses& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const AvailableDevicesAddresses& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "AvailableDevicesAddresses{";
      _aidl_os << "values: " << ::android::internal::ToString(values);
      _aidl_os << ", logic: " << ::android::internal::ToString(logic);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  enum class Tag : int32_t {
    availableInputDevices = 0,
    availableOutputDevices = 1,
    availableInputDevicesAddresses = 2,
    availableOutputDevicesAddresses = 3,
    telephonyMode = 4,
    forceConfigForUse = 5,
  };

  // Expose tag symbols for legacy code
  static const inline Tag availableInputDevices = Tag::availableInputDevices;
  static const inline Tag availableOutputDevices = Tag::availableOutputDevices;
  static const inline Tag availableInputDevicesAddresses = Tag::availableInputDevicesAddresses;
  static const inline Tag availableOutputDevicesAddresses = Tag::availableOutputDevicesAddresses;
  static const inline Tag telephonyMode = Tag::telephonyMode;
  static const inline Tag forceConfigForUse = Tag::forceConfigForUse;

  template<typename _Tp>
  static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, AudioHalCapCriterionV2>;

  AudioHalCapCriterionV2() : _value(std::in_place_index<static_cast<size_t>(availableInputDevices)>, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevices()) { }

  template <typename _Tp, typename = std::enable_if_t<
      _not_self<_Tp> &&
      std::is_constructible_v<std::variant<::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevices, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevices, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevicesAddresses, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevicesAddresses, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::TelephonyMode, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::ForceConfigForUse>, _Tp>
    >>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr AudioHalCapCriterionV2(_Tp&& _arg)
      : _value(std::forward<_Tp>(_arg)) {}

  template <size_t _Np, typename... _Tp>
  constexpr explicit AudioHalCapCriterionV2(std::in_place_index_t<_Np>, _Tp&&... _args)
      : _value(std::in_place_index<_Np>, std::forward<_Tp>(_args)...) {}

  template <Tag _tag, typename... _Tp>
  static AudioHalCapCriterionV2 make(_Tp&&... _args) {
    return AudioHalCapCriterionV2(std::in_place_index<static_cast<size_t>(_tag)>, std::forward<_Tp>(_args)...);
  }

  template <Tag _tag, typename _Tp, typename... _Up>
  static AudioHalCapCriterionV2 make(std::initializer_list<_Tp> _il, _Up&&... _args) {
    return AudioHalCapCriterionV2(std::in_place_index<static_cast<size_t>(_tag)>, std::move(_il), std::forward<_Up>(_args)...);
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

  inline bool operator==(const AudioHalCapCriterionV2& _rhs) const {
    return _value == _rhs._value;
  }
  inline bool operator<(const AudioHalCapCriterionV2& _rhs) const {
    return _value < _rhs._value;
  }
  inline bool operator!=(const AudioHalCapCriterionV2& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const AudioHalCapCriterionV2& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const AudioHalCapCriterionV2& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const AudioHalCapCriterionV2& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream os;
    os << "AudioHalCapCriterionV2{";
    switch (getTag()) {
    case availableInputDevices: os << "availableInputDevices: " << ::android::internal::ToString(get<availableInputDevices>()); break;
    case availableOutputDevices: os << "availableOutputDevices: " << ::android::internal::ToString(get<availableOutputDevices>()); break;
    case availableInputDevicesAddresses: os << "availableInputDevicesAddresses: " << ::android::internal::ToString(get<availableInputDevicesAddresses>()); break;
    case availableOutputDevicesAddresses: os << "availableOutputDevicesAddresses: " << ::android::internal::ToString(get<availableOutputDevicesAddresses>()); break;
    case telephonyMode: os << "telephonyMode: " << ::android::internal::ToString(get<telephonyMode>()); break;
    case forceConfigForUse: os << "forceConfigForUse: " << ::android::internal::ToString(get<forceConfigForUse>()); break;
    }
    os << "}";
    return os.str();
  }
private:
  std::variant<::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevices, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevices, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevicesAddresses, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::AvailableDevicesAddresses, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::TelephonyMode, ::aidl::android::media::audio::common::AudioHalCapCriterionV2::ForceConfigForUse> _value;
};
}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace common {
[[nodiscard]] static inline std::string toString(AudioHalCapCriterionV2::LogicalDisjunction val) {
  switch(val) {
  case AudioHalCapCriterionV2::LogicalDisjunction::EXCLUSIVE:
    return "EXCLUSIVE";
  case AudioHalCapCriterionV2::LogicalDisjunction::INCLUSIVE:
    return "INCLUSIVE";
  default:
    return std::to_string(static_cast<int8_t>(val));
  }
}
}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction, 2> enum_values<aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction> = {
  aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction::EXCLUSIVE,
  aidl::android::media::audio::common::AudioHalCapCriterionV2::LogicalDisjunction::INCLUSIVE,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace media {
namespace audio {
namespace common {
[[nodiscard]] static inline std::string toString(AudioHalCapCriterionV2::Tag val) {
  switch(val) {
  case AudioHalCapCriterionV2::Tag::availableInputDevices:
    return "availableInputDevices";
  case AudioHalCapCriterionV2::Tag::availableOutputDevices:
    return "availableOutputDevices";
  case AudioHalCapCriterionV2::Tag::availableInputDevicesAddresses:
    return "availableInputDevicesAddresses";
  case AudioHalCapCriterionV2::Tag::availableOutputDevicesAddresses:
    return "availableOutputDevicesAddresses";
  case AudioHalCapCriterionV2::Tag::telephonyMode:
    return "telephonyMode";
  case AudioHalCapCriterionV2::Tag::forceConfigForUse:
    return "forceConfigForUse";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace common
}  // namespace audio
}  // namespace media
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag, 6> enum_values<aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag> = {
  aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag::availableInputDevices,
  aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag::availableOutputDevices,
  aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag::availableInputDevicesAddresses,
  aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag::availableOutputDevicesAddresses,
  aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag::telephonyMode,
  aidl::android::media::audio::common::AudioHalCapCriterionV2::Tag::forceConfigForUse,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
