#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
"""Turns build/measure.jsonl (tests/measure-engine.sh) into the app's
app/app/src/main/assets/measurements.json: every effect's measured response and numbers, shown
on the app's "Engine measurements" page and published with each release.

    scripts/gen-measurements.py [build/measure.jsonl] [engine version]
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build/measure.jsonl"
version = sys.argv[2] if len(sys.argv) > 2 else re.search(
    r'RFX_VERSION_NAME "([^"]+)"', (ROOT / "engine/CMakeLists.txt").read_text()).group(1)

lines = [json.loads(line) for line in source.read_text().splitlines() if line.strip()]
out = {
    "format": "resonancefx.measurements.v1",
    "engine": version,
    "sampleRate": 48000,
    "method": "Each effect alone, at the settings listed, driven through the Android effect interface "
              "exactly as the phone does. Response: steady sines at -24 dBFS. Loudness: pink noise at "
              "-20 dBFS. Distortion: THD (2nd-6th harmonics) of a -12 dBFS sine. Dynamics: a 1 kHz sine "
              "at -3 and -40 dBFS. Width: side/mid energy of a partly correlated signal. Tail: energy "
              "0.3-1.3 s after a click.",
    "freqs": lines[0]["freqs"],
    "effects": lines[1:],
}
target = ROOT / "app/app/src/main/assets/measurements.json"
target.write_text(json.dumps(out, separators=(",", ":")) + "\n")
print(f"{target.relative_to(ROOT)}: {len(out['effects'])} measurements, engine {version}")
