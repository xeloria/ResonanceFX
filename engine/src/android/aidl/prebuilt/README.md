# AIDL interface prebuilts

`libresonancefx_aidl.so` links against the platform's frozen
`android.hardware.audio.effect-V?` interface library. The AIDL toolchain does **not**
generate that library (the NDK backend emits headers only), and it is not in the NDK, so a
copy is needed at link time.

Only `arm64-v8a` is present: that is the ABI of the Android 16 device in issue #5
(Samsung SM-A576B, `a57x`). Add other ABIs the same way if a build for them is needed.

## arm64-v8a / android.hardware.audio.effect-V3-ndk.so

| | |
| --- | --- |
| Device | Samsung Galaxy A57 5G, SM-A576B (`a57x`), Android 16 / One UI 8.5 |
| Firmware | `A576BXXS4AZG5`, build `BP4A.251205.006` |
| Source | `https://gitlab.com/Van-Firmware-Dumps/samsung/a57x_26425/-/blob/a57xnaxx-user-16-BP4A.251205.006-A576BXXS4AZG5-release-keys/system/system/lib64/android.hardware.audio.effect-V3-ndk.so` |
| Path on device | `/system/lib64/android.hardware.audio.effect-V3-ndk.so` |
| Size | 329 872 bytes |
| SHA-256 | `091534d767fd61806732125a5e0e1dc38427ac76969adbc5df2de88c6a138276` |
| ELF | ELF64, AArch64, shared object |
| SONAME | `android.hardware.audio.effect-V3-ndk.so` |
| Build ID | `5ef3137a5faad32369690b9b956239d1` |

Dependencies as reported by `llvm-readelf -d`: `libbinder_ndk.so`, `libc++.so`, `libc.so`,
`libm.so`, `libdl.so`, `android.media.audio.common.types-V4-ndk.so`,
`android.media.audio.eraser.types-V1-ndk.so`, `android.hardware.audio.common-V4-ndk.so`,
`android.hardware.common-V2-ndk.so`, `android.hardware.common.fmq-V1-ndk.so`.

Reviewed before adding: 433 exported symbols, including `BnEffect::{BnEffect,~BnEffect,
createBinder,getInterfaceVersion,getInterfaceHash}`, `DefaultExtension::{descriptor,
readFromParcel,writeToParcel}`, `VendorExtension`, and the `IEffect`/`IFactory` defaults. The
file carries the Android ident and build-id notes and no non-AOSP imports.

Note: the `Android-Dumps/samsung/a57x` mirror of the same firmware was truncated
(317 584 bytes, section header table past EOF). The `Van-Firmware-Dumps` copy above is
complete (section table ends exactly at EOF).

**Link-time only.** The module does not ship this library; at runtime the effect HAL process
already has its own copy, and our library records it as `DT_NEEDED`. If a device's copy differs
in symbols the library will fail to load there, which is exactly the compatibility check we
want.

AOSP `hardware/interfaces/audio/aidl` is Apache-2.0; see `third_party/aosp/LICENSE`.
