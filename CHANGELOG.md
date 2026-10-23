# Changelog

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
