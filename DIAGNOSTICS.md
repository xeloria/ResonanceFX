# Diagnostics

ResonanceFX names every failure with a stable code, so a user can report **one code** instead of
a paragraph of symptoms. The same code is used by:

- the module description in Magisk / KernelSU / APatch (after boot),
- the module's **Action** button (code, reason, detail, fix),
- the app's Status screen and the copied status report.

The module also publishes the full verdict to the app's own files directory
(`module-diagnosis.txt`), because the app cannot read `/data/adb`. The app validates the format
and the boot id, so a copy left by a previous boot or a removed module is ignored; when no copy
exists it falls back to the smaller app-side diagnosis.

The decision tree lives in `module/common.sh` (`rfx_diagnose`) and the app's half in
`driver/Diagnosis.kt`. They share the vocabulary but not the scope: the module runs as root and
can inspect processes and logcat; the app is unprivileged and can only report what it can read.

## Codes

| Code | Meaning | What to do |
| --- | --- | --- |
| `OK` | The effect factory registered the engine. | Nothing. Attachment and processing are separate checks. |
| `E02` | No engine build exists for this CPU. | Report the device's ABI list. |
| `E03` | The module is installed, but its files are not visible to the audio HAL. | Reboot. If it persists, set the root manager to the global mount namespace, then reinstall. |
| `E04` | No audio-effects config that the HAL reads was found. | Report the ROM name and the logs; the module does not know its config path. |
| `E05` | A config was found but could not be patched. | Send the ROM's `audio_effects.xml`; its shape is unsupported. |
| `E06` | The config the HAL reads has no ResonanceFX entry. | Reinstall 2.6.0 or newer; if it persists, send that config and the logs. |
| `E07` | The ROM uses the AIDL effect HAL, but this build has no AIDL engine for its API level. | Update the module. |
| `E08` | The config entry does not match the factory generation (AIDL vs legacy). | Reinstall the module. |
| `E09` | The HAL cannot find the engine library the config names. | Reinstall; if it persists, send the logs. |
| `E10` | The HAL found the library but cannot load it: linker namespace, CPU ABI, dependencies, symbols or SELinux. | Send the log line; it names the exact reason. |
| `E11` | The HAL rejected the patched config. | Send the config and the logs. |
| `E12` | *App:* the engine is registered but not attached to a stream. | Play audio, tap Re-apply, check the session error. |
| `E13` | *App:* the engine service is not running. | Turn the master switch on, or reopen the app. |
| `E14` | The module is installed and the config/library look correct, but the factory has not registered the engine. | Tap Action again or reboot; if it persists, send the logs. |
| `E15` | The effect-HAL generation could not be determined (audio may still be starting). | Wait a few seconds and tap Action again. |
| `E16` | The Action runs in a private PID namespace, so the module cannot see the audio processes. | Run Action from the root manager's own screen, or reboot. |

## What each check reads

1. **Factory generation** — which effect libraries the running audio processes have mapped
   (`/proc/<pid>/maps`). AIDL and legacy plug-ins use different ABIs and entry shapes. The
   *loader* processes (those that map a stock plug-in) are preferred over every process whose
   executable happens to contain “audio”, so a file visible to audioserver but not to the
   factory is not counted as visible.
2. **The config** — the AOSP-effective file is the first readable
   `audio_effects.xml` / `audio_effects_config.xml` in `/odm/etc`,
   `/vendor/etc/audio/sku_<ro.boot.product.vendor.sku>`, `/vendor/etc`, `/system/etc`
   (or the legacy `/vendor/etc/audio_effects.conf` on Android 7 and older). Vendor factories
   sometimes read a different file (Xiaomi `/my_product`, some MTK builds), so E06 is only
   reported when **none** of the configs the module patched contains the entry; the detail names
   both the AOSP-effective file and every carrier.
3. **The entry and its library** — the config's `library` and `path` attributes are read back,
   and the path is resolved exactly like the factory does: relative names are searched in
   `/odm`, `/vendor` and `/system` `<libdir>/soundfx`, as seen through the loader's
   `/proc/<pid>/root` (not through the root shell's own view). Both `lib64` and `lib` are tried,
   so a 32-bit HAL on a 64-bit device is not mistaken for a missing library.
4. **The file itself** — ELF class and machine are checked against the directory it resolved
   from, not against the device's primary ABI.
5. **Factory logs** — `dlopen`/namespace/parse errors are classified (`NS`, `ABI`, `SYM`, `DEP`,
   `SELINUX`, `MISSING`, `PARSE`). These lines are saved to `logs/logcat.txt` by the boot stage.
6. **Registration** — `dumpsys media.audio_flinger` must list the effect UUID.

## Why a code can still be E14

`E14` means everything the module can check is in place, but the factory has not registered the
engine. That is often a timing issue (the HAL is still starting) or a vendor-specific behaviour
the module cannot observe. Tapping **Action** restarts audio and re-checks; if it stays `E14`,
the logs are the next step.

## Limits

The tree covers every failure class we have seen across AOSP, Qualcomm/SKU, Samsung and
Android 5–17, but it is not literally universal:

- **Private PID namespaces** (some KernelSU/APatch sessions) hide the audio processes from the
  Action shell; the module then reports `E16` instead of guessing.
- **A 32-bit AIDL effect HAL** cannot load the shipped AIDL front-end, which is built for
  64-bit only. The module detects that case and reports `E07` directly; the fix is a 32-bit AIDL
  build, which this release does not provide.
- **Custom vendor factories** that read a config no one else reads cannot be confirmed. The
  module now only says `E06` when *no* patched config contains the entry; if the entry exists
  somewhere, the result is `E14` with the candidate list and the factory log line, rather than a
  wrong accusation.
- **ROMs that read several config files** (Samsung) work because the module patches all of them,
  but the E06 check follows the AOSP “first readable” rule. If a vendor reads a different file
  first, the log line is still the source of truth.
- **The app** cannot read `/data/adb`, `/proc` or logcat. It reports the observable subset
  (`E04`, `E06`, `E07`, `E12`, `E13`, `E14`) and points at the Action for the rest.
- **No script runs at all** if the module is disabled or not mounted. The app then shows `E04`
  or `E06`; the module card keeps the last written description.

## Reporting an issue

Include the code from the module description (or the Action output) and
`/data/adb/resonancefx/logs`. The Action prints a `Detail:` line and a `Fix:` line; those are
usually enough to identify the ROM quirk without a back-and-forth.
