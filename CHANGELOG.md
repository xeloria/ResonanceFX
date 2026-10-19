# Changelog

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
