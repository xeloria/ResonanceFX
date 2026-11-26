# 📜 Licensing

ResonanceFX is **GPL-3.0-or-later**, Copyright (C) 2026 Xeloria. Every source file carries an
SPDX header; `python scripts/add-spdx.py --check` verifies it.

> ℹ️ This page is a plain-language summary to help you read the license. It is **not legal
> advice** and **not a substitute** for [`LICENSE`](../LICENSE), which is the actual, canonical
> GPLv3 text and always wins.

## 🧭 At a glance

- 🏠 **Using it yourself?** No obligations. Install it, modify it, keep your changes private.
- 📤 **Sharing it?** Share the source too, under the same license, with the notices intact.
- 💰 **Selling it?** Allowed — but the people you ship to get the same freedoms you did.
- 🛡️ **No warranty.** The authors are not liable if it breaks something.

## ✅ You can

- 🎧 **Use it for anything** — personal, commercial, on as many devices as you like.
- ✏️ **Modify it** and keep your changes to yourself, as long as you don't distribute them.
- 📤 **Distribute copies**, modified or not, for free or for a fee.
- 🔧 **Combine it** with other code whose license is compatible with GPLv3.
- 🧾 **Choose a later version** — "or-later" means GPLv4+ when the FSF publishes one.

## 📤 If you share it, you must

- 📚 **Provide the complete corresponding source** for the version you ship — including the
  scripts and build files needed to produce it (GPLv3 §6).
- 🔁 **License your changes under GPL-3.0-or-later** (or a compatible later version).
- 🧾 **Keep the notices**: copyright lines, SPDX headers, [`NOTICE`](../NOTICE) and the license
  text itself.
- 🏷️ **Mark what you changed**, so recipients know it is not the original.
- 🚫 **Add no extra restrictions** — no "you may not redistribute", no added fees, no DRM
  loopholes (§10).
- 📲 **For user products** (devices that can't easily run modified software), pass along the
  information needed to install a modified version (§6, the "anti-tivoization" rule).

## 🚫 You can't

- 🙅 **Ship binaries without source** to the people who receive them.
- 🔒 **Lock it down** with terms that take away the freedoms GPLv3 grants.
- 🏷️ **Use the ResonanceFX name or logo** as if the project endorsed you — the GPL grants
  copyright permissions, not trademark rights.
- ⚖️ **Hold the authors liable** for damages; §15 and §16 disclaim warranty and liability.

## 🗺️ Finding your way around the GPL

The full text is long because it is precise. This is what each section actually covers:

| § | What it says |
| --- | --- |
| 0 | 📖 Definitions — "source code", "convey", "propagate", and friends |
| 1 | 💻 What counts as source code |
| 2 | ✅ The basic permissions: run, study, modify, distribute |
| 3 | 🛡️ No warranty; protecting users from patent traps |
| 4 | 📦 Conveying verbatim copies |
| 5 | ✏️ Conveying modified source — the "share your changes" rule |
| 6 | 🏗️ Conveying binaries — source offer and install information |
| 7 | ➕ Additional terms that are allowed (attribution, notices) |
| 8 | ⛔ Termination if you break the terms, and how to cure it |
| 9 | ✔️ You don't need to "accept" to receive a copy |
| 10 | 🚫 No additional restrictions may be layered on top |
| 11 | 🧪 Patent license and the patent-litigation termination |
| 12 | 🕊️ No surrender of others' freedom |
| 13 | 🤝 Combining with AGPLv3 code |
| 14 | 🔁 Revised versions of the GPL |
| 15 | ⚠️ Disclaimer of warranty |
| 16 | 🧯 Limitation of liability |
| 17 | 🧭 How to interpret conflicts with other agreements |

## 🧩 What the code is

- **Android glue** (`engine/src/android`, `Engine.cpp`, `Protocol.h`): written for ResonanceFX
  against the public Android audio effect API (`android_effect_abi.h` declares that ABI).
- **DSP core** (`engine/src/core`): from Basilar, also by Xeloria, GPL-3.0-or-later.
- **Effects** (`engine/src/fx` and the modules built on the core): original work.
- **Root module** (`module/`) and **app** (`app/`): original work.

Every effect is checked with `tests/measure-engine.sh` (frequency response, distortion, stereo
width, decay) so DSP changes can't silently alter the sound.

## 📦 Third-party data

- MIT KEMAR HRIRs (Gardner & Martin, 1994): free to use with citation (see [`NOTICE`](../NOTICE)).
- AutoEq profiles: MIT License (see `app/app/src/main/assets/autoeq/LICENSE`).
- AOSP AIDL effect front-end: Apache-2.0 (see `engine/src/android/aidl/third_party/aosp/`).

## 🔗 Canonical text

- 📄 [`LICENSE`](../LICENSE) — GPLv3, the authoritative text (do not edit it).
- 🧾 [`NOTICE`](../NOTICE) — copyright and third-party attributions.
- 🌐 <https://www.gnu.org/licenses/gpl-3.0.html> — the FSF's copy and FAQ.
