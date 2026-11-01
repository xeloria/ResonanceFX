# 🤝 Contributing

Thanks for helping! 🎉 Bug reports, device reports and pull requests are all welcome.

## 🐛 Reporting a problem

Use the issue templates. **Every bug report needs a `logcat` captured while the problem
happens.** Reports without one are guesswork — with one they're usually a ten-minute fix. The
well-dressed bug report contains:

1. 📋 The report from the app's **Status** tab (**Copy report**), pasted into the issue.
2. 📝 **A `logcat` from the moment it misbehaves** — not from after you restarted:

   ```sh
   adb logcat -c                        # optional: start from a clean buffer
   # ... reproduce the problem now ...
   adb logcat -d -v time -b main -b system > logcat.txt
   ```

   Don't filter or trim it manually; attach the whole file (it's small). If you can, include a
   couple of seconds *before* and *after* the glitch. For reports about stutter or audio cuts,
   the exact period of the interruption (every N seconds, or on tapping something) matters, so
   note it next to the file.
3. 📦 All files from `/data/adb/resonancefx/logs/` (`install.log`, `boot.log`, `action.log` and,
   if the module wrote it, `logcat.txt`).
4. 📱 Device model, Android version, root manager (Magisk / KernelSU / APatch), and the
   ResonanceFX **version of both the module and the app** (they are shown in Settings and on
   the Status tab).
5. 🎧 What you were playing through (headphones, Bluetooth device model, speaker) and whether
   it happens with effects **on** and **off** — several reported issues turn out to be
   screen- or output-specific, and that split is the fastest way to isolate them.

### 💡 Tips

- Don't have USB debugging on? Settings → About → tap *Build number* 7 times, then Developer
  options → USB debugging, then `adb` from [platform-tools](https://developer.android.com/tools/releases/platform-tools).
- If the app itself crashes, a tombstone/stack from the logcat above is enough — no need to
  deobfuscate anything, our mapping file is applied on request.
- Hardware bugs you can't reproduce? Still file it with the Status report + logs: patterns
  across devices are how the module's device quirks get catalogued.

## 🔧 Pull requests

- ✅ Every source file starts with an SPDX header; `python scripts/add-spdx.py` adds it to new
  files. Contributions are accepted under GPL-3.0-or-later.
- ✅ Run the tests that cover your change (see the README's *Build* section). CI runs the
  patcher, engine host tests (ASan/UBSan), app unit tests and a full build on every pull
  request.
- 🔈 Engine changes that affect the sound: include before/after output of
  `tests/measure-engine.sh` for the effects you touched.
- ⏱️ Keep the engine real-time safe: no allocation, locks or logging on the audio thread.
- 🔐 Don't submit code copied from projects without a GPL-3.0-compatible license (see
  [`docs/LICENSING.md`](docs/LICENSING.md)).

## 🚢 Releases

1. 🏷️ Bump the version in `module/module.prop` (`version`, `versionCode`),
   `app/app/build.gradle.kts` (`versionName`, `versionCode`) and `engine/CMakeLists.txt` +
   `scripts/build-engine.sh` (engine version), and add a section to `CHANGELOG.md`.
2. 📈 Run `tests/measure-engine.sh` (Linux / WSL, about 25 minutes). It refreshes
   `app/app/src/main/assets/measurements.json`, the engine measurements the app shows and the
   release publishes. Commit it with the release.
3. 📦 Commit, then tag and push: `git tag v2.2.1 && git push origin v2.2.1`.
4. 🚀 The **Release** workflow builds the module and the app and publishes both, plus the
   measurements, with checksums.

For a stable app signature, add these repository secrets once (Settings → Secrets and
variables → Actions): `RFX_KEYSTORE_BASE64` (`base64 -w0 release.jks`), `RFX_KEYSTORE_PASSWORD`,
`RFX_KEY_ALIAS`, `RFX_KEY_PASSWORD`. Without them each release is signed with a different
throwaway key and users have to uninstall before updating.
