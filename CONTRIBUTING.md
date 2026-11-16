# Contributing

Thanks for helping! Bug reports, device reports and pull requests are all welcome.

## Reporting a problem

Use the issue templates. For "the engine doesn't load", please attach:

- the report from the app's **Status** tab (**Copy report**),
- the files in `/data/adb/resonancefx/logs/` (`install.log`, `boot.log`, `action.log`),
- your device model, Android version and root manager (Magisk, KernelSU or APatch).

## Pull requests

- Every source file starts with an SPDX header; `python scripts/add-spdx.py` adds it to new
  files. Contributions are accepted under GPL-3.0-or-later.
- Run the tests that cover your change (see the README's *Build* section). CI runs the patcher,
  engine host tests (ASan/UBSan), app unit tests and a full build on every pull request.
- Engine changes that affect the sound: include before/after output of
  `tests/measure-engine.sh` for the effects you touched.
- Keep the engine real-time safe: no allocation, locks or logging on the audio thread.
- Don't submit code copied from projects without a GPL-3.0-compatible license (see
  [`docs/LICENSING.md`](docs/LICENSING.md)).

## Releases

1. Bump the version in `module/module.prop` (`version`, `versionCode`),
   `app/app/build.gradle.kts` (`versionName`, `versionCode`) and `scripts/build-engine.sh`
   and `engine/CMakeLists.txt` (engine version), and add a section to `CHANGELOG.md`.
2. Run `tests/measure-engine.sh` (Linux / WSL, about 25 minutes). It refreshes
   `app/app/src/main/assets/measurements.json`, the engine measurements the app shows and the
   release publishes. Commit it with the release.
3. Commit, then tag and push: `git tag v2.0.1 && git push origin v2.0.1`.
4. The **Release** workflow builds the module and the app and publishes both, plus the
   measurements, with checksums.

For a stable app signature, add these repository secrets once (Settings → Secrets and variables
→ Actions): `RFX_KEYSTORE_BASE64` (`base64 -w0 release.jks`), `RFX_KEYSTORE_PASSWORD`,
`RFX_KEY_ALIAS`, `RFX_KEY_PASSWORD`. Without them each release is signed with a different
throwaway key and users have to uninstall before updating.
