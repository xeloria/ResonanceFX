# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
"""Prints build/measure.jsonl (see tests/measure-engine.sh): per scenario, the frequency
response and the numbers that matter for listening (loudness, distortion, width, decay)."""
import json
import sys
from pathlib import Path

SHOW = [40, 100, 250, 1000, 4000, 10000, 16000]
LABELS = [("noiseDb", "loudness dB"), ("thd100", "THD@100Hz %"), ("thd1k", "THD@1k %"),
          ("widthDb", "width S/M dB"), ("tailDb", "tail dB"), ("gain1kLoud", "gain loud"),
          ("gain1kQuiet", "gain quiet"), ("noiseLoudDb", "noise loud"), ("noiseQuietDb", "noise quiet")]

lines = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines() if line.strip()]
freqs = lines[0]["freqs"]
idx = [freqs.index(f) for f in SHOW]
fmt = lambda v: "  --  " if v is None else f"{v:+6.1f}"

for d in lines[1:]:
    print(f"\n== {d['name']}  ({d['settings']})")
    print("   Hz       " + "".join(f"{f:>7}" for f in SHOW))
    print("   response " + "".join(f"{fmt(d['response'][i]):>7}" for i in idx))
    extras = [f"{label}: {d[key]:.2f}" for key, label in LABELS if d.get(key) is not None]
    if extras:
        print("   " + " | ".join(extras))
    if "crosstalk" in d:
        print("   crosstalk" + "".join(f"{fmt(d['crosstalk'][i]):>7}" for i in idx))
