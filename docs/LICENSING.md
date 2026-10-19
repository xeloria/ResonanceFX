# Licensing

ResonanceFX is GPL-3.0-or-later, Copyright (C) 2026 Xeloria. Every source file carries an SPDX
header; `python scripts/add-spdx.py --check` verifies it.

## What the code is

- **Android glue** (`engine/src/android`, `Engine.cpp`, `Protocol.h`): written for ResonanceFX
  against the public Android audio effect API (`android_effect_abi.h` declares that ABI).
- **DSP core** (`engine/src/core`): from Basilar, also by Xeloria, GPL-3.0-or-later.
- **Effects** (`engine/src/fx` and the modules built on the core): original work.
- **Root module** (`module/`) and **app** (`app/`): original work.

Every effect is checked with `tests/measure-engine.sh` (frequency response, distortion, stereo
width, decay) so DSP changes can't silently alter the sound.

## Third-party data

- MIT KEMAR HRIRs (Gardner & Martin, 1994): free to use with citation (see NOTICE).
- AutoEq profiles: MIT License (see `app/app/src/main/assets/autoeq/LICENSE`).
