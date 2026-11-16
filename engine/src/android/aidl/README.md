# AIDL effect front-end (TODO)

This directory is the drop-in point for the engine's **AIDL effect** build. It is empty on
purpose: the module already detects which factory a ROM uses and selects the right library
(`module/common.sh`), so adding sources here is all that is needed to enable AIDL devices.

## Why it is needed

The library in `src/android/EffectLibrary.cpp` exports the legacy `audio_effect_library_t`
(`AELI`) ABI. That is what HIDL effect HALs and AIDL HALs that kept native effect libraries
load. The AOSP AIDL effect factory (`hardware/interfaces/audio/aidl/default/EffectFactory.cpp`)
does not know that ABI: it dlopens the libraries listed in `/vendor/etc/audio_effects_config.xml`
and looks up

```c
extern "C" binder_exception_t createEffect(const AudioUuid& implUuid,
                                           std::shared_ptr<IEffect>* out);
extern "C" binder_exception_t queryEffect(const AudioUuid& implUuid, Descriptor* out);
extern "C" binder_exception_t destroyEffect(const std::shared_ptr<IEffect>& instance);
```

and calls them through `aidl::android::hardware::audio::effect::IEffect` (FMQ based). A phone
whose factory is the AIDL one cannot register the legacy engine, which is the "UUID not found in
HAL" failure seen on Android 16 Samsung builds.

## Contract

When this directory contains `*.cpp`, `engine/CMakeLists.txt` builds `libresonancefx_aidl.so`
from the shared DSP core plus these sources, and `scripts/build-module.sh` packages it next to
the legacy library. The module then:

- detects the AIDL factory and lists the AIDL library in `audio_effects_config*.xml` with
  `type="4daa8eaa-0842-4a8d-82e9-6eee5827fbe4"` (the attribute the AIDL parser requires for
  effects that are not one of Android's built-in types);
- leaves the AIDL config untouched when no AIDL build is packaged, so a failing legacy entry can
  never make the factory drop its whole effect list.

The app talks to the engine through raw `AudioEffect.setParameter(int, byte[])` payloads. The AIDL
side must keep that wire format by exposing it as `Parameter::Specific::vendorEffect`
(`CommandId::SET_PARAM_VENDOR` / `GET_PARAM_VENDOR`); the framework's
`AidlConversionVendorExtension` maps the legacy `effect_param_t` calls onto it, so the app and
saved presets need no changes. DSP calls can reuse `EffectContext`.

## Versioning

The AIDL plug-in is compiled against a specific
`android.hardware.audio.effect` interface version. The interface is versioned per Android
release, so ship one `libresonancefx_aidl.so` per supported platform (build per target API) and
let the installer pick it by `ro.build.version.sdk`; the current single-file name is the default
when only one platform is packaged. Needs the NDK, the AIDL compiler and the platform AIDL
interface libraries (`android.hardware.audio.effect-V?-ndk`, `libbinder_ndk`, `libfmq`).
