# 🔒 Security Policy

ResonanceFX runs inside Android's audio server with root-installed files, so memory-safety
defects in the engine (for example malformed parameters, impulse responses or profile files)
matter. The module's mount-and-patch scripts are root code as well.

Our goal is to permit users with root to safely install this module. We work to fix all
vulnerabilities reported to us.

## 📌 Supported versions

| Version | Support |
|---|---|
| 2.2.1 | ✅ Fully supported |
| < 2.2.1 | ❌ Only security-relevant fixes are backported if feasible |

## 📬 How to report

Please report vulnerabilities privately through GitHub's
[security advisories](https://github.com/xeloria/ResonanceFX/security/advisories/new) instead
of a public issue — public reports give attackers time and give you no credit process.

Include:

- 🏷️ the ResonanceFX version (module **and** app, from the Status tab),
- 📱 the device model, Android version and root manager (Magisk / KernelSU / APatch),
- 🔁 steps to reproduce — ideally as a file, stream, or parameter sequence we can feed
  through the commander (`adb logcat -v time` output of the reproduction helps enormously,
  see [`CONTRIBUTING.md`](CONTRIBUTING.md#-reporting-a-problem)),
- 🧪 any malformed input that triggers it (a crafted `.wav` impulse response, `.vdc` profile,
  AutoEQ JSON, or preset …), attached directly.

## 🎯 In scope

- Out-of-bounds, integer-overflow and race conditions reachable through the effect command
  interface (parameter payloads, kernel upload chunks/commits, ABX controls).
- The file parsers the engine and the app feed into it: impulse-response WAVs, `.vdc`
  headset-correction profiles, AutoEQ/preset JSON, per-device profile storage.
- Channel-layout, sample-format and rate handling in the Android glue.
- The module scripts: config patching, mount/bind handling and the uninstall path, including
  cross-recovery-session behaviour.
- The app's asset import/validation flow and profile save path.

## 🚫 Out of scope

- Anything requiring an already-compromised device, or other apps doing malicious things with
  their own permissions.
- Vendor HAL bugs: if the stock `audio_effects` pipeline mishandles a vendor configuration
  without ResonanceFX installed, that's a platform report — but tell us what you found and how
  it interacts with the module and we'll help triage.
- Social attacks or denial-of-service of the reporting infrastructure.

## 🧪 What we do about it

- The engine builds with AddressSanitizer + UBSan in CI-style host runs
  (`tests/run-engine-tests.sh`), and every setParameter/getParameter path is bounds-checked
  against the declared payload sizes before use.
- The audio path is allocation-free and lock-free by design (try-to-lock, never stall the
  output thread); command handling serialises with the processing path.
- Output is saturating (never wraps) and takes the same bounds-checking on every format.
- Reports get a fix assessment quickly, a fix in the next patch release, and public credit in
  the changelog unless you prefer otherwise.
