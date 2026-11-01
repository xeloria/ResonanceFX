# Changelog

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
