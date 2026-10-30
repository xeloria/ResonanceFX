# ResonanceFX

Created by **Xeloria**.

A free and open-source, privacy-respecting system-wide audio effects suite for rooted Android:
a hardened DSP engine, a universal root module that installs it on any Android 5+ device, and a
modern Material You app.

## Highlights

- **28 effect modules**: 10-band equalizer (56 presets), 15/31-band graphic EQ, parametric EQ
  (up to 32 bands), AutoEQ (thousands of headphones), loudness compensation (ISO 226, follows
  the volume), multiband compressor, dynamic EQ, de-esser, vocals & ambience (centre
  extraction), spatial audio (MIT KEMAR HRTF, head tracking on Android 13+), convolver (impulse responses,
  auto-resampled to the live output rate), headphone correction (`.vdc` profiles), FET
  compressor, playback gain control, bass and clarity enhancers, field/differential surround,
  headphone surround, reverb, dynamic system (103 headphone profiles), tube simulator, analog
  coloration, crossfeed, output gain/balance/limiter.
- **Made for speakers too**: speaker correction (phone, tablet, laptop, Bluetooth, car and TV
  profiles), **measure your own speaker** with the phone's microphone and get a correction fitted
  to it, speaker protection (an excursion limiter that holds back bass a small driver can't
  play, or limited at each band's measured distortion point) and speaker widening (crosstalk
  cancellation for closely spaced speakers, following how the device is held).
- **Blind test**: a built-in ABX test, loudness-matched and time-aligned, to check whether you
  really hear an effect.
- **Measured, not claimed**: every effect's measured response, distortion and loudness change
  ships in the app and with each release.
- **Per-device profiles** that switch automatically for speaker, wired, every Bluetooth device
  and USB DAC; JSON presets with import/export.
- **Live status**: see whether the engine is registered, attached and processing, with a
  built-in test tone and a copyable diagnostics report.
- **Robust engine**: any channel layout (1–28 ch), PCM 16/24/32-bit, 8.24 and float, saturating
  output (no wrap-around crackle), real-time-safe locking, bounds-checked commands; arm, arm64,
  x86, x86_64, 16 KB pages.
- **Self-healing**: re-attaches after audioserver restarts and re-uploads impulse responses when
  the output sample rate changes.
- **Private**: no ads, no analytics, no network permission. The microphone is only used when
  you measure a speaker, and the recording never leaves memory.

## Install

Download both files from the [latest release](https://github.com/xeloria/ResonanceFX/releases/latest).

1. Flash `ResonanceFX-<version>.zip` in Magisk, KernelSU or APatch and reboot.
2. Install `ResonanceFX.apk`, open it and turn effects on.
3. The **Status** tab confirms: engine registered → attached → stream accepted → processing.

If the engine doesn't load, tap the module's **Action** button in your root manager: it
re-patches every config, re-applies mounts, restarts the audio services and prints what it found.
Logs: `/data/adb/resonancefx/logs/` (`install.log`, `boot.log`, `logcat.txt`, `action.log`).
After every boot the module description shows a ✅/⚠️/❌ verdict, confirmed against the audio
HAL's list of registered effects.

## How the module loads reliably

- Finds every `audio_effects*.xml|.conf` (HIDL, AIDL `audio_effects_config.xml`, SKU variants,
  vendor-specific names) on `/odm`, `/vendor`, `/system_ext`, `/product`, `/my_product`,
  `/mi_ext` (and `/system` on Android 7 and older), and adds the effect to all of them: which
  files a HAL loads is vendor-specific.
- Re-generates the patched copies from the *current* stock files on every boot, so OTA updates
  never leave stale configs.
- Patching is idempotent and tolerant: removes stale entries, handles inline and self-closing
  `<libraries/>`, ignores commented-out tags, reuses the soundfx path in legacy `.conf` files.
- Uses the root manager's own overlay first, verifies after mounting and bind-mounts only what's
  missing, with SELinux labels copied from the originals.
- Installs the 32-bit engine too when the device has 32-bit userspace (many 64-bit phones still
  run a 32-bit audio HAL).

## Build

Requirements: Android SDK with NDK 27+, CMake 3.22+, JDK 21; Linux/WSL g++ for engine tests.

```sh
scripts/build-engine.sh                  # out/engine/<abi>/libresonancefx.so
scripts/build-module.sh                  # out/ResonanceFX-<version>.zip
(cd app && ./gradlew assembleRelease)    # app/app/build/outputs/apk/release/

tests/test-patch.sh                      # config patcher (add "busybox sh" to run under busybox)
tests/run-engine-tests.sh                # engine through AELI with ASan/UBSan (Linux/WSL)
tests/measure-engine.sh                  # per-effect measurements (Linux/WSL)
tests/emulator-test.sh                   # end to end on a rooted emulator/device over adb
(cd app && ./gradlew testDebugUnitTest lintDebug)
```

Release APKs are signed with the key given in `RFX_KEYSTORE`, `RFX_KEYSTORE_PASSWORD`,
`RFX_KEY_ALIAS` and `RFX_KEY_PASSWORD`; without them the debug key is used. Pushing a `v*` tag
builds the module and app on GitHub Actions and publishes a release (see
[`CONTRIBUTING.md`](CONTRIBUTING.md#releases)).

## Layout

- `engine/` — engine (`src/android/` effect glue, `src/core/` DSP core, `src/fx/` effects),
  `engine/tests/` host harness
- `module/` — root module scripts (`common.sh` holds all logic)
- `app/` — Android app (Kotlin, Jetpack Compose)
- `docs/` — licensing notes
- `.github/` — CI, release workflow, issue templates
- `tests/` — patcher fixtures and test runners

## License

GPL-3.0-or-later. See [`LICENSE`](LICENSE), [`NOTICE`](NOTICE) for third-party data
attributions, and [`docs/LICENSING.md`](docs/LICENSING.md) for how the engine was written.
