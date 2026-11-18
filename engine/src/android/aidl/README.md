# AIDL effect front-end (Android 16+)

Android 16's audio effect HAL (`vendor.audio-effect-hal-aidl`) loads only **AIDL** effect
plug-ins. It `dlopen`s the library named in `audio_effects_config.xml` and looks up
`createEffect`, `queryEffect` and `destroyEffect`; the legacy `audio_effect_library_t` (AELI)
entry points used by HIDL HALs are not recognised. This directory adds that front-end; the DSP,
the parameter protocol and the stream conversion stay in `src/android/EffectContext.cpp`.

## Layout

| Path | Contents |
| --- | --- |
| `RfxAidlEffect.cpp` | `RfxAidlEffect : EffectImpl`, `RfxAidlContext`, the three exports |
| `NativeHandle.cpp` | `native_handle_*` helpers (libcutils is not in the NDK) |
| `third_party/aosp/` | AOSP `EffectImpl`, `EffectContext`, `EffectThread`, `effect-impl` headers, FMQ headers, `EventFlag.cpp`, `FmqInternal.cpp` (Apache-2.0) |
| `third_party/platform/` | Minimal `android-base`, `utils/*`, `cutils/*`, `system/*` shims for an out-of-tree NDK build |
| `generated/` | Headers generated from the frozen `android.hardware.audio.effect` **V3** (API 36) API |

## Contract mapping

* `Parameter::Common` → `effect_config_t`: float32, same rate, the AIDL channel layout mask,
  `EFFECT_BUFFER_ACCESS_READ`/`WRITE`.
* `Parameter::Specific::vendorEffect` → `DefaultExtension.bytes` is the complete legacy
  `effect_param_t` (AIDL V2 and later), so it goes straight to `EFFECT_CMD_SET_PARAM` /
  `EFFECT_CMD_GET_PARAM`. The app's `AudioEffect.setParameter` payload and saved presets are
  unchanged.
* The float FMQ block is passed as `audio_buffer_t { frameCount = samples / channels, f32 }`.
* `START`/`STOP`/`RESET` → `EFFECT_CMD_ENABLE`/`DISABLE`/`RESET`.

The descriptor reports type `ec7178ec-e5e1-4432-a3f4-4657e6795210` and implementation
`4daa8eaa-0842-4a8d-82e9-6eee5827fbe4`. `module/common.sh` writes the **type** UUID into the
`type="..."` attribute of the AIDL config entry (not the implementation UUID).

## Building

The AIDL build needs three things the NDK does not ship:

1. the platform's frozen interface library for the target ABI, e.g.
   `/system/lib64/android.hardware.audio.effect-V3-ndk.so` (Android 16);
2. `libcutils.so` for the same ABI (FMQ uses `ashmem_create_region`);
3. a `libfmq` implementation built with the NDK's libc++ — vendored here (`EventFlag.cpp`,
   `FmqInternal.cpp`), because the platform `libfmq.so` is built with platform libc++
   (`std::__1`) and cannot be mixed with NDK `std::__2` types.

Put them in `build/device-libs/<abi>/` and run `scripts/build-engine.sh`; the AIDL library is
packaged as `libresonancefx_aidl_<api>.so` (API from `ro.build.version.sdk`, 36 on Android 16)
so one module zip can carry several platform versions.

Without those files the CMake target is skipped and the legacy build is unaffected.

## Vendored AOSP changes

* `EffectImpl.cpp`: `destroyEffect` marked `default` visibility (built with
  `-fvisibility=hidden`); the NDK backend has no `getInterfaceVersion()`, so the interface
  version is the build-time constant 3; the duplicate `"include/effect-impl/..."` includes were
  normalised.
* `EffectContext.cpp`: same include normalisation.
* `third_party/platform/Utils.h`: `utils/FastStrcmp.h` is used (`fasticmp`), so it is included.
* `third_party/platform/android-base/*`, `utils/*`: small shims, documented in each file.

Everything under `third_party/aosp` is Apache-2.0, Copyright (C) The Android Open Source
Project; see `NOTICE`.
