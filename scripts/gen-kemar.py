#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
"""Regenerates engine/src/core/src/data/KemarHrir.cpp from the MIT KEMAR archive.

The archive is Gardner, W. G. & Martin, K. D. (1994), MIT Media Lab Perceptual Computing
Technical Report #280, and is free to use with citation. Download

    https://sound.media.mit.edu/resources/KEMAR/full.zip

extract it, and point --input at the extracted "full" directory (the one holding elev-40 ...
elev90 and headphones+spkr).

The table keeps one pinna (left ear) for both ears, which is the arrangement the archive's
documentation recommends: a source at azimuth A is L(A) for the left ear and L(360 - A) for
the right ear. That makes the median plane (0 and 180 degrees) exactly symmetric, so a source
crossing in front of the listener does not swap two different filters.

Output layout: kHrir[elevation][azimuth][ear][tap], signed 16-bit, 44.1 kHz, 128 taps,
elevations -40..90 in 10-degree steps, azimuths 0..180 in 5-degree steps (the renderer
mirrors the left half). The measured azimuth increments get coarser towards the pole, so the
5-degree grid is filled by circular linear interpolation between adjacent measurements.
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import wave

ELEVATIONS = list(range(-40, 91, 10))
AZIMUTHS = list(range(0, 181, 5))
TAPS = 128
SAMPLE_RATE = 44100
# Measurements per elevation in the archive (hrtfdoc.txt, table 1), used as a sanity check.
EXPECTED = {-40: 56, -30: 60, -20: 72, -10: 72, 0: 72, 10: 72, 20: 72, 30: 60,
            40: 56, 50: 45, 60: 36, 70: 24, 80: 12, 90: 1}


def read_wav(path: str) -> list[int]:
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            raise SystemExit(f"{path}: expected 16-bit samples")
        if w.getnchannels() != 1:
            raise SystemExit(f"{path}: expected mono")
        raw = w.readframes(w.getnframes())
    return list(struct.unpack("<%dh" % (len(raw) // 2), raw))


def load_elevation(root: str, elev: int) -> dict[int, list[int]]:
    folder = os.path.join(root, f"elev{elev}")
    pattern = re.compile(r"^L-?\d+e(\d{3})a\.wav$")
    measured: dict[int, list[int]] = {}
    for name in sorted(os.listdir(folder)):
        m = pattern.match(name)
        if not m:
            continue
        measured[int(m.group(1))] = read_wav(os.path.join(folder, name))
    if len(measured) != EXPECTED[elev]:
        raise SystemExit(f"elev{elev}: found {len(measured)} measurements, expected {EXPECTED[elev]}")
    return measured


def interpolate(measured: dict[int, list[int]], target: int) -> list[int]:
    """Circular linear interpolation of one direction at a 5-degree azimuth grid point."""
    azimuths = sorted(measured)
    if len(azimuths) == 1:  # directly overhead: every azimuth is the same measurement
        return measured[azimuths[0]]
    if target in measured:
        return measured[target]
    before = max(a for a in azimuths if a < target)
    after = min((a for a in azimuths if a > target), default=azimuths[0] + 360)
    next_az = after if after < 360 else after - 360
    t = (target - before) / float(after - before)
    a, b = measured[before], measured[next_az]
    n = min(len(a), len(b))
    return [a[i] + t * (b[i] - a[i]) for i in range(n)]


def onset(ir: list[int]) -> int:
    peak = max(abs(v) for v in ir)
    return next(i for i, v in enumerate(ir) if abs(v) > 0.2 * peak)


def extract(ir: list[int], crop: int) -> list[int]:
    """Crops to the table length. The archive is onset-aligned per direction (the 1.4 m air
    delay is already removed), so one global crop preserves the interaural delay differences
    while keeping as much of the response tail as possible."""
    if len(ir) < crop + TAPS:
        raise SystemExit(f"measurement has {len(ir)} taps, expected at least {crop + TAPS}")
    return ir[crop:crop + TAPS]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default=os.path.join("build", "kemar", "full"),
                        help="extracted MIT KEMAR 'full' directory")
    parser.add_argument("--output", default=os.path.join("engine", "src", "core", "src", "data",
                                                         "KemarHrir.cpp"))
    args = parser.parse_args()

    if not os.path.isdir(os.path.join(args.input, "elev0")):
        raise SystemExit(f"{args.input}: no elev0 directory; point --input at the extracted "
                         "KEMAR 'full' folder")

    measured_by_elevation = {elev: load_elevation(args.input, elev) for elev in ELEVATIONS}
    # Leave four samples before the earliest onset so the pre-ringing of every direction is
    # kept, then truncate to the table length once, globally.
    crop = max(0, min(onset(ir) for m in measured_by_elevation.values() for ir in m.values()) - 4)

    table = []
    for elev in ELEVATIONS:
        measured = measured_by_elevation[elev]
        for az in AZIMUTHS:
            ir = extract(interpolate(measured, az), crop)
            # Left ear for the left channel, the mirrored direction for the right ear.
            mirror = extract(interpolate(measured, (360 - az) % 360), crop)
            table.append((elev, az, ir, mirror))

    # The median plane must be exactly symmetric or a source panned across the front swaps
    # filters mid-way. It is by construction (same measurement for both ears at 0 and 180).
    for elev, az, left, right in table:
        if az in (0, 180) and left != right:
            raise SystemExit(f"elev{elev} az{az}: median plane is not symmetric")

    lines = [
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "// Copyright (C) 2026 Xeloria",
        "",
        "// GENERATED by scripts/gen-kemar.py - do not edit.",
        "// MIT KEMAR HRIRs, 710 positions: Gardner, W. G. & Martin, K. D. (1994), MIT Media Lab",
        "// Perceptual Computing Technical Report #280. Free to use with citation.",
        f"// {len(ELEVATIONS)} elevations ({ELEVATIONS[0]}..{ELEVATIONS[-1]} deg, "
        f"{ELEVATIONS[1] - ELEVATIONS[0]} deg steps), {len(AZIMUTHS)} azimuths (0..180 deg, 5 deg",
        f"// steps), {TAPS} taps at {SAMPLE_RATE} Hz, signed 16-bit, cropped {crop} samples",
        "// before the earliest onset. One pinna, mirrored.",
        '#include "rfx/data/Kemar.h"',
        "",
        "namespace rfx {",
        "namespace kemar {",
        "const int16_t kHrir[kElevations][kAzimuths][2][kTaps] = {",
    ]
    for elev in ELEVATIONS:
        rows = [t for t in table if t[0] == elev]
        lines.append(f"    {{  // elevation {elev} deg")
        for _, az, left, right in rows:
            lines.append(f"        {{  // azimuth {az} deg")
            for ear, ir in enumerate((left, right)):
                values = ", ".join(str(int(round(v))) for v in ir)
                lines.append(f"            {{{values}}},")
            lines.append("        },")
        lines.append("    },")
    lines += ["};", "", "}  // namespace kemar", "}  // namespace rfx", ""]

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print(f"wrote {args.output}: {len(table)} directions x 2 ears x {TAPS} taps (crop {crop})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
