# Changelog

## v2.8.0

Fixed:

- **The legacy engine is installed in `/system/lib64/soundfx`, not `/vendor`.** The system-
  namespace audioserver cannot `dlopen` `/vendor/lib64/soundfx`, and the legacy `.conf` loader
  picks `/vendor` first and then has it rejected by the linker namespace. Both audioserver and
  the vendor audio HAL can load `/system`, so the engine now loads in both. This is the root
  cause of the S24 / OneUI 9 failure in #17.
- **`/system` effect configs are only patched when they are the config the framework reads.**
  The decision is made from the pre-mount snapshot, not from the Action's mount namespace, so an
  Action can no longer create a late `/system/etc/audio_effects.xml` mount that audioserver
  cannot use (and that native detectors can see). Vendor and SKU configs already carry the
  effect.
- **The Action no longer deletes live bind sources.** The config stage keeps its inodes: no
  `rm -rf $RFX_STAGE/cfg`, and patchers overwrite in place instead of `mv`. A run can no longer
  make `/vendor/etc/audio_effects_sec.xml` vanish with `XML_ERROR_FILE_NOT_FOUND`.
- **Bootloops on Samsung / KernelSU Next.** `post-mount` applies overlay-only binds and memoises
  empty process scans; the stock-library copy and any re-apply happen after `sys.boot_completed`,
  so a slow operation cannot trip the boot watchdog.
- **Diagnosis accuracy.** `FILE_NOT_FOUND` reports `E06`, `dlopen … not found` and
  `could not get library` report `E09`, and framework noise (`AirCmd`, package lists) no longer
  produces a bogus `E10`. The Action and app describe the actual candidate paths.

Changed:

- **App `E13` / `E12`:** app-side guidance instead of “open the module Action”. `E13` shows the
  master-switch state and a **Start engine** button that turns the master on, starts
  `EngineService` and refreshes the tile. The app trusts a factory registration it can see over
  a stale module `E14`.
- The copied status report includes `Master switch: true/false`.
- Version 2.8.0 (20800). Engine DSP is unchanged; the measurements differ only in the reported
  engine version.

## v2.7.0

Added:

- **A stable diagnosis code for every failure.** The boot stage, the Action button and the app's
  Status screen now run one ordered decision tree and report the same code (`E03` files not
  mounted, `E04` no config, `E06` the config the HAL reads has no entry, `E07` AIDL engine
  missing, `E09`/`E10` library not found or not loadable, `E11` config rejected, `E14` not
  registered yet, and so on). The module description in the root manager shows the code after
  boot, and the Action prints the reason, the detail and the fix. Codes and checks are documented
  in `DIAGNOSTICS.md`.
- The app reports its observable half of the same diagnosis (`E04`, `E06`, `E07`, `E12`, `E13`,
  `E14`) on the Status screen and in the copied report.
- The diagnosis covers pre-Android 8 `.conf`-only ROMs, 32-bit audio HALs on 64-bit devices, and
  reports `E16` when a private PID namespace hides the audio processes from the Action.
- `E06` is now only reported when *no* config the module patched carries the entry, so vendor
  configs outside the AOSP search order (Xiaomi `/my_product`, some MTK builds) are not
  misdiagnosed. Visibility is judged from the processes that actually map effect plug-ins, not
  from every process whose name contains “audio”.
- A 32-bit AIDL effect HAL is reported as `E07` (the shipped AIDL front-end is 64-bit only)
  instead of surfacing as a misleading library error.
- The module publishes its verdict to the app's files directory with a boot id, so the app can
  show the same exact code and reason instead of only its own subset. Stale copies are ignored.
- Regression tests for every code and for the config-entry, ELF, factory-error, 32-bit resolver
  and published-verdict helpers.

Changed:

- The installer names `E07`, `E04` and `E05` directly instead of a generic failure message.
- Version 2.7.0 (20700). Engine DSP is unchanged; the measurements differ only in the reported
  engine version.

## v2.6.0

Fixed:

- **The Action button no longer loses the effect.** It deleted the generated config copies and
  then re-discovered the ROM's configs from the mounted system; once the module's own copy was
  gone, discovery could come up empty and the factory lost ResonanceFX until the next reboot
  (#11). The boot stage now snapshots the stock configs before mounting, and regeneration always
  patches from that snapshot.
- **`/system/etc/audio_effects.xml` is patched when it is the config the framework reads.** Some
  ROMs ship no `/odm` or `/vendor` config, so the factory falls back to the framework copy (and
  so does AudioPolicy). It is still left alone when a vendor or SKU config takes precedence.
- **The Action verdict is honest when nothing was patched.** An empty config list used to be
  reported as "library and configs are visible"; it now names the real problem.
- **The app detects QTI SKU configs** (`/vendor/etc/audio/sku_*`) and the `/system` copy, so the
  Status screen no longer says "Effect factory: not installed" when the module is loaded.

Changed:

- **Master gain and limiter use 0.5 dB steps.** The recovered vendor tables jumped from -6.0 dB
  to -10.5 dB (and the limiter had six positions); both sliders now span 0.5 dB steps from
  -40.0 to +6.0 dB and -20.0 to 0.0 dB (#12). Settings move to format v3: existing profiles and
  presets are migrated by their old table index and never shift by more than 0.25 dB.
- Slider values round to the nearest step instead of truncating, so negative ranges feel the
  same in both directions.
- Version 2.6.0 (20600). Engine DSP is unchanged; the measurements differ only in the reported
  engine version.

## v2.5.1

Fixed:

- **AIDL install on Android 17 and newer.** The installer now selects the newest AIDL build
  that is not newer than the running release (the AIDL interface library is frozen), so the
  API 36 engine is used on API 37 instead of concluding that no AIDL engine is shipped.
  Previously the AIDL configs were left untouched and the HAL never saw the effect.
- **AIDL engine location for APEX effect HALs.** The AIDL build is installed only in
  `/system/lib64/soundfx`; an APEX-hosted effect HAL (Samsung/Exynos) permits `/system/lib64`
  but not `/vendor/lib64`, and the AOSP factory searches `/odm`, `/vendor`, `/system` in that
  order. The legacy AELI build stays in `/vendor`, and stale AIDL copies are removed there.
- **Preset deletion asks for confirmation** in the app, naming the preset; Cancel, back or an
  outside tap leaves it in place.

Changed:

- Version 2.5.1 (20501). Engine DSP is unchanged; the measurements differ only in the reported
  engine version.
- Source builds of the AIDL front-end include the minimal `system/graphics.h` shim needed by
  the vendored `thread_defs.h`.
- CI and release workflows skip the removed legacy `tools` package, and the host test harness
  no longer tries to compile the AIDL front-end.

## v2.5.0

Added:

- **Android 16+ AIDL effect HAL support.** The engine now ships
  `libresonancefx_aidl_<api>.so`, a front-end for ROMs whose effect factory only loads AIDL
  plug-ins (Android 16 One UI 8.5 and newer). The parameter payload, presets and DSP are
  unchanged; HIDL/legacy ROMs keep using `libresonancefx.so`.
- **Differential surround: wider and mono-safe.** The Haas effect now delays only the side
  signal, keeping the centre image intact. A 120 Hz high-pass on the side removes the
  low-frequency cancellation of the old full-channel delay, width changes are ramped over
  20 ms, delay changes keep the click-free dip, and near-mono material bypasses the delay
  automatically. **Width** is exposed in the effect; the high-pass and mono guard stay at
  their safe defaults. Contributed by @xiamoYuF.

Changed:

- Version 2.5.0 (20500). This includes the Haas refactor that the 2.4.0 snapshot had
  replaced; 2.4.0 itself stays as published.

## v2.4.0

Added:

- **App: six languages.** Japanese, Russian, Simplified Chinese, Spanish, Korean and French,
  on top of English. The app follows the device language automatically, and **Settings →
  Language** overrides it (including "Follow system"); Android 13+ also lists the app under
  Settings → Apps → Language. Everything is translated: effect names and descriptions, every
  control, the status page, speaker measurement, presets, errors, and the 41 engine
  measurement entries with their settings descriptions.
- The engine measurement page now reads its prose from resources keyed by the English effect
  name, so the numbers stay in one generated file and translations cannot drift from them.

Changed:

- The four Status-page action buttons now share the row width, so "Engine measurements" is no
  longer wider than the others (and translated labels wrap without mismatched heights).

## v2.3.0

Changed:

- **Engine**: spatial audio now tracks the head in three degrees of freedom. The app sends the
  head-tracker quaternion (not just yaw), the renderer rotates every virtual source by the
  inverse orientation and looks it up at its resulting azimuth *and elevation*. The MIT KEMAR
  table was regenerated from the complete 710-position archive (14 elevations from -40 to +90
  degrees, 37 azimuths, one pinna mirrored for a symmetric median plane) and is interpolated
  bilinearly; the previous table only had the horizontal plane. Filters and interaural delays
  are still split into minimum-phase responses and pure delays, and changes are crossfaded per
  32-sample block. The yaw-only parameter keeps working for older apps, and the renderer's
  compensation EQ is now measured from the existing table instead of preparing a second one.
- **App**: head tracking sends the orientation quaternion when the engine advertises
  `kFeatureHeadOrientation`, and falls back to yaw for older engines. Only the heading is
  recentred (12 s time constant); pitch and roll stay gravity-referenced, so a deliberate tilt
  is not slowly cancelled. Updates are sent about 30 times a second when the orientation moved
  by 0.2 degrees.
- **Data**: `scripts/gen-kemar.py` regenerates the HRIR table from the MIT archive. The
  horizontal layer is now derived from the same full set as the other elevations (the old
  table came from the separate compact set), so the default timbre may differ slightly; the
  compensation EQ re-fits itself to it.

## v2.2.2

Fixed:

- **Module**: the engine is now verified from the effect process's own view (`/proc/<pid>/root`,
  for `audioserver` and every audio HAL), not from the module's shell. On KernelSU Next a bind
  made from a root shell can live in a private mount namespace; the module used to report
  "library and configs in place" while the HAL could not see `libresonancefx.so` at all
  (`EffectsFactoryConfigLoader: Could not find library in effect directories`). The boot log,
  the module description and the Action report now say `HIDDEN` when the file exists in the
  shell but not for the audio HAL.
- **Module**: bind mounts are drained layer by layer (up to four `umount`s, and a mount check
  before touching anything) instead of a single `umount`, so an update can no longer inherit a
  stale bind from the previous version and leave the effect registered to a deleted file.
- **Module**: installing over an existing module now drops its binds and stage/state first.
- **Module**: the description and Action verdict distinguish "AIDL engine required",
  "audio HAL cannot see the library", "configs not visible to the audio HAL" and "HAL did not
  register", instead of blaming the configs in every case.

Changed:

- **Module**: the installer and every boot detect which effect factory the ROM runs — the
  legacy/HIDL ABI (`audio_effect_library_t`) or the AIDL factory that only loads AIDL plug-ins —
  and patch accordingly: legacy configs get the AELI engine, AIDL configs get the AIDL engine plus
  the `type="..."` attribute the AIDL config parser requires. If the ROM uses the AIDL factory and
  this build has no AIDL engine, its config is left untouched (listing a legacy library there makes
  the factory drop its whole effect list) and the module description says so. The engine build is
  picked up automatically once `out/engine/<abi>/libresonancefx_aidl.so` exists.
- **App**: the Status screen and the copied report show which effect factory the module targeted
  (AIDL, HIDL/legacy or both), read back from the config the module patched.
- **Engine**: 2.2.2 build.

## v2.2.1

Changed:

- **Engine**: the clarity/tube/clipper oversampler now runs its anti-aliasing FIR on *both*
  phases (the odd phase used to be passed through unfiltered), so the nonlinear stages no
  longer receive a half-filtered image band. Passband behaviour at 1 Hz is unchanged; the
  measured stereo/distortion pages will be slightly lower near the cutoff, which is the point.
- **Engine**: 16-bit output is dithered (TPDF, first-order noise shaping), removing
  quantisation harmonics on quiet passages. Float, 24/32-bit and 8.24 output are unchanged.
- **Engine**: about 1000 lines of dead code (stereo image, spectral denoiser, transient
  shaper, tape/vinyl, declicker, per-channel delay, obsolete payload decoders) removed.
- **App**: one shared engine-status poll (2 s) now feeds the Effects, AutoEQ and Status screens
  instead of three independent binder probes (one per visible AutoEQ row).
- **App**: the test tone generates and writes its audio off the main thread; the status button
  and the service command no longer risk a stretched frame or an ANR on slow devices.
- **App**: per-device profiles and presets keep their exact names — the old parameter-legal
  sanitiser mapped different outputs onto one file; new files carry a short hash when the
  sanitiser actually lost characters, and existing profiles keep working (migrating on the
  next save).

Fixed:

- **App**: removed the "Blind test" (ABX listening test) option; the Effects/Status screens
  and its pieces are gone from the app (the engine keeps the capability, unused).
- **App**: a phantom blank area below the filter row on the Effects tab — with effects on, a
  chip added next to a long output name could be squeezed into the few pixels left over,
  wrapping its label into a tall invisible column that pushed every following
  card down. The filter row now wraps chips onto the next line instead.
- **Engine**: gain/pan ramps no longer restart from their current value every buffer — with
  short buffers a 20 ms ramp stretched to an asymptotic crawl; parameter transitions are linear
  and complete on time again.
- **Engine**: modules with built-in latency (convolver, vocals/ambience, clarity, tube,
  compressor clipper) no longer produce a flanged transient and time jump when switching them
  on or off — the crossfade mixes the dry signal delayed by the module's own latency.
- **Engine**: the pass-through path used while the audio thread can't lock no longer reads the
  stream configuration without synchronization (also respects a rejected stream/disable while
  passing through instead of running on stale settings).
- **Engine**: impulse-responses with a float count not divisible by the channel path count are
  rejected instead of being committed silently truncated; the compressor's adaptive timing
  restarts with a stream reset; reverb LFO phases reset in sync; re-routing the excursion
  limiter no longer ticks old filter state with new coefficients; changing the field-sound
  delay engages in a 6 ms dip instead of clicking; after each boot the engine's reporting clock
  is monotonic.
- **App**: dragging on the equalizer/cross-over curve survives the first committed 0.5 dB step
  (the gesture used to be cancelled by its own recomposition).
- **App**: selecting "None" (or deleting) the headset-correction profile now clears the
  engine's coefficients — the old correction kept being applied.
- **App**: a convolution kernel is only resampled once the framework has configured the output;
  the watchdog now corrects any kernel applied before that, even if it sampled the rate only
  after configuration. A failed or late inquiry shows "waiting for the output sample rate"
  instead of silently resampling against the wrong rate.
- **App**: attachment reports failure and frees the session slot instead of keeping a
  half-applied engine instance; failed profile saves now log and retry in place instead of
  being silently dropped; importing a file that fails parsing leaves existing assets with a
  colliding name untouched.
- **App**: the engine-status watchdog probes at 3 s only while audio can be flowing (an
  instance enabled, an attach retrying, or a handle just replaced) and backs off to 10 s after
  two consecutive idle probes — the periodic probe commands take the effect chain's lock, and
  on a suspended/offloaded Bluetooth output that is audible as a short stutter every few
  seconds. Any enabled/attach outcome resets the backoff, so recovery to the fast cadence is
  immediate.
- **App**: the Status/Effects screens no longer poll the effect handle from the UI. The
  foreground service publishes a status snapshot from its own watchdog tick, and the screens
  read that (with a single one-shot probe as fallback when the service isn't running). The
  old per-screen poll loops ran getter bursts every 1.5–5 s against the effect, and on a
  suspended/offloaded output those are audible — the reporters saw a stutter every few
  seconds while sitting on the Effects/Status tab with effects switched off, gone on the
  Presets/Settings tabs that never touch the handle.
- **App**: a listening-test trial count that could never reach significance no longer throws;
  the hero card stops animating while effects are off.
- **Module**: boot diagnostics no longer mis-report the mount mode when only part of the
  system is bind-mounted (`[ a ] || [ b ] && c` parsing); bind-mounts are removed before a new
  one replaces them (no more stacked binds whose stage directories outlive them); uninstall
  drains double-stacked binds with two passes; recovery sessions (ROM flashing, recovery-mode
  boots) skip the mount stages instead of writing boot logs from them.

## v2.2.0

- **Blind test**: find out whether you really hear an effect. A is without it, B with it, X is
  secretly one of them; the engine plays them time-aligned and at matched loudness (BS.1770,
  3 s moving average), so only the effect itself can give X away. Test all effects or a single
  one, 10/16/20 trials, with the statistical verdict (binomial, p < 0.05) and a history.
- **Engine measurements**: every effect's measured frequency response, loudness change,
  distortion, dynamics, stereo width and decay, shown in the app (Status tab and About) and
  published with each release. Measured, not claimed.
- **Sound like another speaker**: refit a measured speaker's correction so it matches another
  speaker you measured, within the range both can play.
- **Speaker widening follows the device**: on the built-in speakers it fades in when the phone is
  held sideways and out when upright (where the speakers are above and below you).
- **Distortion-aware speaker protection**: the speaker measurement can also play rising sweeps
  (opt-in, stops as soon as the speaker distorts) to find, per band, the loudest level below 5 %
  distortion. Speaker protection's new "Measured" mode then limits each band at its own measured
  ceiling.
- **Bass enhancer, finer control**: the Gain slider is now stepless (0.0-17.0 dB in 0.1 dB
  steps, so small phone speakers get a usable low end instead of a +3.5 dB minimum), and a new
  Harmonics slider (0.00-0.50, Pure bass+ only) sets how much virtual bass is generated - set
  Gain to 0 and still hear low end, without moving the driver. Existing bass gain settings are
  converted automatically.

## v2.1.0

Speakers get the same care as headphones.

- **Speaker correction**: profiles for phone, tablet, laptop, small and large Bluetooth
  speakers, car and TV, with a strength control (replaces the single "speaker optimization").
- **Measure your speaker**: the app plays two sweeps, records them with the phone's microphone
  and fits a parametric correction to the result (at most +6 dB of boost, only within the range
  the speaker can play). Effects pause while measuring; the recording never leaves memory.
- **Speaker protection**: an excursion limiter that holds back loud bass below the driver's
  resonance, so small speakers play louder without rattling or distorting.
- **Speaker widening**: crosstalk cancellation that makes stereo reach past the edges of a
  phone, tablet or laptop, set by speaker spacing and listening distance.
- New permission: microphone (only asked for when you start a measurement).

Fixes:

- **Playback gain control** keeps its level through seeks and track changes. Android disables,
  reconfigures and re-enables effects around every seek on some outputs (seen on the Galaxy S24
  Ultra speaker), which used to restart the levelling from 0 dB.
- Playback gain levels a new track within about 0.3 s instead of 5-10 s, then follows smoothly.
- Playback gain only lifts quiet tracks by default; "Turn loud tracks down" allows cuts. The
  default target is now -14 LUFS (it was -24.5 LUFS, which turned normal music down, most
  audibly on Bluetooth earbuds).
- The level-dependent modules (playback gain, compressors, dynamic EQ, de-esser) judge the music
  itself instead of the music after the volume knob, on outputs where Android applies the volume
  before effects (speaker, wired, USB).
- The app re-attaches by itself if attaching failed while the audio system was restarting.

## v2.0.0

A new engine, written from scratch and licensed GPL-3.0-or-later.

- **New modules**: loudness compensation (ISO 226, follows the system volume), parametric EQ
  (up to 32 bands: peaks, shelves, passes, notches), 15/31-band graphic EQ, multiband
  compressor, dynamic EQ, de-esser, vocals & ambience (centre extraction: dialogue boost or
  karaoke), spatial audio (MIT KEMAR HRTF, head tracking on Android 13+).
- **Better existing effects**: the FET compressor and playback gain control now act on the
  signal, reverb no longer adds a large level jump, crossfeed strengths are in the right order,
  tube simulation stays clean at normal levels.
- Smooth 20 ms crossfades when switching any effect on or off.
- AutoEQ: measured correction for thousands of headphones (from v1.1.0).
- Sections for modules an older engine doesn't support are hidden in the app.
