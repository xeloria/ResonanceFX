<div align="center">

# 🎧 ResonanceFX

**System-wide audio, exactly how you want to hear it.**

A free, open-source, privacy-respecting audio effects suite for **rooted Android** — a hardened
real-time DSP engine, a universal Magisk / KernelSU / APatch module, and a modern Material You app.

[![Latest release](https://img.shields.io/github/v/release/xeloria/ResonanceFX?style=for-the-badge&label=latest&color=7C4DFF)](https://github.com/xeloria/ResonanceFX/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/xeloria/ResonanceFX/total?style=for-the-badge&label=downloads&color=00C853)](https://github.com/xeloria/ResonanceFX/releases)
[![License](https://img.shields.io/github/license/xeloria/ResonanceFX?style=for-the-badge&color=FFB300)](LICENSE)
[![Android 5.0+](https://img.shields.io/badge/android-5.0%2B-3DDC84?style=for-the-badge&logo=android&logoColor=white)](#-compatibility)
[![Root](https://img.shields.io/badge/root-Magisk%20%7C%20KernelSU%20%7C%20APatch-FF6D00?style=for-the-badge)](#-compatibility)
[![Languages](https://img.shields.io/badge/languages-7-9C27B0?style=for-the-badge)](#-speaks-your-language)

[**⬇️ Download**](https://github.com/xeloria/ResonanceFX/releases/latest) &nbsp;•&nbsp;
[**📥 Install**](#-install-in-3-steps) &nbsp;•&nbsp;
[**✨ Features**](#-whats-inside) &nbsp;•&nbsp;
[**🩺 Troubleshooting**](#-troubleshooting) &nbsp;•&nbsp;
[**🔧 Build**](#-build-from-source) &nbsp;•&nbsp;
[**🐞 Report a bug**](https://github.com/xeloria/ResonanceFX/issues/new/choose)

</div>

---

> **🆕 What's new in 2.5.0** — Android 16+ **AIDL effect-HAL support** (One UI 8.5 and newer), and a
> **wider, mono-safe differential surround**. Full details in the [changelog](CHANGELOG.md).

## 💡 Why ResonanceFX?

| 🎚️ **28 effects, one engine** | 🎧 **Headphones *and* speakers** |
| --- | --- |
| Equalizers, dynamics, spatial audio, reverb, saturation and speaker tools — all processing on the system mixer, so **every app** benefits: music, video, games, podcasts. | 710-position MIT KEMAR HRTF spatial audio, headphone correction, AutoEQ, **measure your own speaker**, crosstalk-cancelling speaker widening. |
| **📊 Measured, not claimed** | **🔒 Free, open and private** |
| Every effect's measured frequency response, distortion, stereo width and loudness change ships with the app and with each release — 41 measurements in all. | GPL-3.0-or-later, no ads, no trackers, **no network permission**. The microphone is used only while you measure a speaker, and the audio never leaves memory. |

<div align="center">

| 🎚️ **Effects** | 📚 **Presets** | 🩺 **Status** | ⚙️ **Settings** |
| :---: | :---: | :---: | :---: |
| Every effect, grouped and explained | Saved setups, per-device profiles, JSON import/export | Live engine state and one-tap diagnostics | Language, theme, measurement, defaults |

</div>

## ✨ What's inside

**🎵 Tone & loudness**
10-band equalizer with 56 presets · 15/31-band graphic EQ · parametric EQ with up to 32 bands ·
AutoEQ for thousands of headphones · ISO 226 loudness compensation that follows the volume ·
bass and clarity enhancers · spectrum extension · playback gain control.

**🥁 Dynamics**
Multiband compressor · dynamic EQ · de-esser · FET compressor · auditory protection.

**🌌 Space & immersion**
Spatial audio with 710-position MIT KEMAR HRTF and **3DoF head tracking** on Android 13+ ·
convolver for impulse responses (auto-resampled to the live output rate) · field surround ·
**differential surround** (mid/side Haas, now wider and mono-safe) · headphone surround+ ·
reverberation · crossfeed.

**🎨 Character**
Tube simulator · AnalogX · dynamic system with 103 headphone profiles · headphone correction
from `.vdc` profiles.

**🔊 Speakers**
Speaker correction for phone, tablet, laptop, Bluetooth, car and TV profiles · **measure your
own speaker** with the phone's microphone · speaker protection that stops a small driver from
excursing too far or distorting · speaker widening with crosstalk cancellation that follows how
the device is held.

**🛠️ Built to live on a phone**
Per-device profiles that switch automatically for speaker, wired, every Bluetooth device and USB
DAC · JSON presets with import and export · built-in test tone · live status and a copyable
diagnostics report · seven languages · any channel layout from 1 to 28 · PCM 16/24/32-bit, 8.24
and float · saturating output (no wrap-around crackle) · real-time-safe locking · bounds-checked
commands · arm, arm64, x86 and x86_64 · 16 KB page support · automatic re-attach after audioserver
restarts and impulse-response re-upload when the output sample rate changes.

## 📥 Install in 3 steps

<div align="center">

**1. Get both files** → [latest release](https://github.com/xeloria/ResonanceFX/releases/latest)
&nbsp;&nbsp;**2. Flash the module** → reboot &nbsp;&nbsp;**3. Install the app** → turn effects on

</div>

1. **Flash** `ResonanceFX-<version>.zip` in Magisk, KernelSU or APatch (or in TWRP / OrangeFox),
   then reboot.
2. **Install** `ResonanceFX.apk` and open it.
3. Turn effects on in the **Effects** tab. **Status** then walks through the chain:
   **engine registered → attached → stream accepted → processing**.

> 💡 **Updating?** Flash the new zip over the old one, reboot, then update the APK. Profiles and
> presets are kept. If the APK refuses to install, it was signed with a different key — uninstall
> the old app and install the new one.

## 🩺 Troubleshooting

| Symptom | Fix |
| --- | --- |
| **Status says the engine isn't registered** | Tap the module's **Action** button in your root manager. It re-patches every config, re-applies mounts, restarts the audio services and prints what it found. Reboot, then check again. |
| **`UUID not found in HAL and proxy list`** on Android 16 | The ROM only loads AIDL effect plug-ins. Use **2.5.0 or newer**; the module installs `libresonancefx_aidl_<api>.so` automatically. |
| **Effects are on but nothing sounds different** | Make sure audio is playing and the effect switch is on. Try the built-in **test tone** in Status, and check the per-device profile for the output you're using. |
| **Sound stops after a call or a Bluetooth switch** | The engine re-attaches on its own; if it doesn't, tap **Action**, then reboot. |
| **Need help or want to report a bug** | In the app: **Status → Copy report**. Attach that plus the logs below to an issue. |

**Logs:** `/data/adb/resonancefx/logs/` — `install.log`, `boot.log`, `logcat.txt`, `action.log`.
After every boot the module description shows a ✅ / ⚠️ / ❌ verdict, checked against the audio
HAL's own list of registered effects.

## 📋 Compatibility

| Requirement | Details |
| --- | --- |
| **Android** | 5.0+ (API 21+) for the module · 6.0+ (API 23+) for the app |
| **Root** | Magisk 20.4+, KernelSU or APatch |
| **CPU** | arm64-v8a, armeabi-v7a, x86_64, x86 — the 32-bit engine is installed too when the device has 32-bit userspace |
| **Audio HAL** | Legacy/HIDL effect libraries, and AIDL effect plug-ins on Android 16+ |
| **Android 16+ note** | The AIDL engine is currently built for **arm64-v8a**; other ABIs fall back to the legacy engine |

## 🔬 How the module loads reliably

<details>
<summary><b>For the curious — how ResonanceFX survives every ROM, OTA and audio restart</b></summary>

- Finds every `audio_effects*.xml` / `.conf` (HIDL, AIDL `audio_effects_config.xml`, SKU
  variants, vendor-specific names) on `/odm`, `/vendor`, `/system_ext`, `/product`,
  `/my_product` and `/mi_ext` (plus `/system` on Android 7 and older), and adds the effect to
  all of them — which files a HAL loads is vendor-specific.
- Detects whether the ROM's effect factory uses the legacy `audio_effect_library_t` ABI (HIDL
  HALs, and AIDL HALs that kept native effect libraries) or the AIDL plug-in ABI, then targets
  the matching engine build and config entry shape. The AIDL config parser needs `type="…"`
  for effects that are not one of Android's built-in types.
- Re-generates the patched copies from the **current stock files on every boot**, so OTA
  updates never leave stale configs.
- Patching is idempotent and tolerant: it removes stale entries, handles inline and
  self-closing `<libraries/>`, ignores commented-out tags, and reuses the `soundfx` path in
  legacy `.conf` files.
- Uses the root manager's own overlay first, verifies after mounting, then bind-mounts only
  what is missing — with SELinux labels copied from the originals.
- Installs the 32-bit engine too when the device has 32-bit userspace (many 64-bit phones still
  run a 32-bit audio HAL).

</details>

## 🔒 Privacy

No ads. No analytics. No telemetry. The app has **no network permission** at all. The microphone
is opened only while you measure a speaker, and the recording is processed in memory and then
discarded — it is never written to storage or sent anywhere.

## 🌍 Speaks your language

English, 日本語, Русский, 简体中文, Español, 한국어 and Français. The app follows the device
language automatically, and **Settings → Language** overrides it (including *Follow system*).
Everything is translated: effects and their descriptions, controls, status, speaker measurement,
presets, errors, and the measurement page.

Want to add a language? See [`docs/TRANSLATING.md`](docs/TRANSLATING.md) — `scripts/check-translations.py`
keeps every translation in sync with the English source.

## 🔧 Build from source

**Requirements:** Android SDK with NDK 27+, CMake 3.22+, JDK 21; Linux or WSL with `g++` for the
engine tests.

```sh
scripts/build-engine.sh                  # out/engine/<abi>/libresonancefx.so
scripts/build-module.sh                  # out/ResonanceFX-<version>.zip
(cd app && ./gradlew assembleRelease)    # app/app/build/outputs/apk/release/

tests/test-patch.sh                      # config patcher (prefix with "busybox sh" to test busybox)
tests/test-factory.sh                    # legacy vs AIDL factory selection
tests/test-action.sh                     # the module's Action button
tests/run-engine-tests.sh                # engine through AELI with ASan/UBSan (Linux/WSL)
tests/measure-engine.sh                  # per-effect measurements (Linux/WSL)
tests/emulator-test.sh                   # end to end on a rooted emulator/device over adb
python scripts/check-translations.py     # translation completeness
(cd app && ./gradlew testDebugUnitTest lintDebug)
```

The AIDL front-end needs the platform's frozen interface library for the target ABI at link time.
The arm64 copy used for the Galaxy A57 lives in
[`engine/src/android/aidl/prebuilt/`](engine/src/android/aidl/prebuilt/README.md), and the
contract is documented in [`engine/src/android/aidl/README.md`](engine/src/android/aidl/README.md).
It is a build input only — at runtime the effect HAL supplies its own copy from `/system/lib64`.

Release APKs are signed with the key given in `RFX_KEYSTORE`, `RFX_KEYSTORE_PASSWORD`,
`RFX_KEY_ALIAS` and `RFX_KEY_PASSWORD`; without them the debug key is used. Pushing a `v*` tag
builds the module and app on GitHub Actions and publishes a release — see
[`CONTRIBUTING.md`](CONTRIBUTING.md#releases).

## 📁 Project layout

| Path | Contents |
| --- | --- |
| `engine/` | DSP engine — `src/android/` effect glue (legacy AELI + AIDL), `src/core/` DSP core, `src/fx/` effects |
| `module/` | Root module scripts (`common.sh` holds the logic) |
| `app/` | Android app (Kotlin, Jetpack Compose, Material You) |
| `docs/` | Licensing and translation notes |
| `tests/` | Patcher fixtures and test runners |
| `.github/` | CI, release workflow, issue templates |

## 🤝 Contributing

Bug reports, device reports and pull requests are all welcome. Read
[`CONTRIBUTING.md`](CONTRIBUTING.md) for what to attach to a report and what CI checks. Sound
changes should come with before/after `tests/measure-engine.sh` output.

## 🙏 Credits

- **DSP core:** Basilar, also by Xeloria (GPL-3.0-or-later).
- **Differential surround refactor:** [@xiamoYuF](https://github.com/xiamoYuF) (PR #7).
- **HRTF data:** MIT KEMAR measurements — W. G. Gardner and K. D. Martin, *HRTF Measurements of
  a KEMAR Dummy-Head Microphone*, MIT Media Lab, 1994.
- **Headphone profiles:** [AutoEq](https://github.com/jaakkopasanen/AutoEq) by Jaakko Pasanen
  (MIT), measurements by their respective sources.
- **AIDL front-end:** AOSP `EffectImpl` / `EffectContext` / `EffectThread` and FMQ sources
  (Apache-2.0), vendored under `engine/src/android/aidl/third_party/`.
- **Installer conventions:** the Magisk module template by Zackptg5 and contributors.

## 📄 License

GPL-3.0-or-later — see [`LICENSE`](LICENSE), [`NOTICE`](NOTICE) for third-party data
attributions, and [`docs/LICENSING.md`](docs/LICENSING.md) for how the engine was written.

<div align="center">

**Made with 🎧 by [Xeloria](https://github.com/xeloria)**

If ResonanceFX makes your music better, consider giving the repo a ⭐ — it helps other people find it.

</div>
