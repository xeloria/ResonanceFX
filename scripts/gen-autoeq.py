# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
"""Builds the bundled AutoEq database from an AutoEq checkout (https://github.com/jaakkopasanen/AutoEq).

Usage: python scripts/gen-autoeq.py <AutoEq repo> app/app/src/main/assets/autoeq

Writes:
  profiles.json  {"commit", "sources": [...], "forms": [...],
                  "profiles": [[name, source, form, preamp, [[type, fc, gain, q], ...], rig], ...]}
                 rig: measurement rig/folder within the source ("" if none); a source may
                 have measured the same model on several rigs, so (source, rig, name) is the key
                 type: 0 = peaking, 1 = low shelf, 2 = high shelf
  LICENSE        AutoEq's MIT license (required when redistributing its data)
"""
import json
import os
import re
import subprocess
import sys

repo, out_dir = sys.argv[1], sys.argv[2]
results = os.path.join(repo, "results")

# Measurement sources in AutoEq's own order of preference; the rest follow alphabetically.
PREFERRED = ["oratory1990", "crinacle", "Rtings", "Innerfidelity", "Headphone.com Legacy"]
TYPES = {"PK": 0, "LSC": 1, "HSC": 2}
FORMS = ["over-ear", "in-ear", "earbud"]

preamp_re = re.compile(r"^Preamp:\s*(-?[\d.]+)\s*dB", re.I)
filter_re = re.compile(
    r"^Filter\s+\d+:\s+ON\s+(PK|LSC|HSC)\s+Fc\s+([\d.]+)\s*Hz\s+Gain\s+(-?[\d.]+)\s*dB\s+Q\s+([\d.]+)", re.I)


def form_of(parts):
    joined = " ".join(parts).lower()
    for i, f in enumerate(FORMS):
        if f in joined:
            return i
    return 0


profiles, skipped = [], 0
sources = sorted(os.listdir(results), key=lambda s: (PREFERRED.index(s) if s in PREFERRED else len(PREFERRED), s.lower()))
sources = [s for s in sources if os.path.isdir(os.path.join(results, s))]

for si, source in enumerate(sources):
    for base, _, files in os.walk(os.path.join(results, source)):
        for f in files:
            if not f.endswith(" ParametricEQ.txt"):
                continue
            name = f[: -len(" ParametricEQ.txt")]
            rel = os.path.relpath(base, os.path.join(results, source)).split(os.sep)
            preamp, filters, ok = 0.0, [], True
            for line in open(os.path.join(base, f), encoding="utf-8", errors="replace"):
                line = line.strip()
                if not line:
                    continue
                if m := preamp_re.match(line):
                    preamp = float(m.group(1))
                elif m := filter_re.match(line):
                    t, fc, g, q = TYPES[m.group(1).upper()], float(m.group(2)), float(m.group(3)), float(m.group(4))
                    if not (10 <= fc <= 22000 and -30 <= g <= 30 and 0.05 <= q <= 30):
                        ok = False
                    filters.append([t, round(fc, 1), round(g, 2), round(q, 3)])
                elif line.lower().startswith("filter"):
                    ok = False  # unknown filter type: don't guess
            if not ok or not filters or len(filters) > 32 or not (-30 <= preamp <= 0):
                skipped += 1
                continue
            # Kept verbatim: it's part of the unique key (the app shortens it for display).
            rig = "/".join(rel[:-1]) if len(rel) > 1 else ""
            profiles.append([name, si, form_of(rel), round(preamp, 2), filters, rig])

profiles.sort(key=lambda p: (p[0].lower(), p[1], p[5]))
keys = [(p[1], p[5], p[0]) for p in profiles]
assert len(keys) == len(set(keys)), "duplicate (source, rig, name) keys"
commit = subprocess.run(["git", "-C", repo, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()

os.makedirs(out_dir, exist_ok=True)
with open(os.path.join(out_dir, "profiles.json"), "w", encoding="utf-8", newline="\n") as fp:
    json.dump({"commit": commit, "sources": sources, "forms": FORMS, "profiles": profiles}, fp,
              ensure_ascii=False, separators=(",", ":"))
license_text = subprocess.run(["git", "-C", repo, "show", "HEAD:LICENSE"], capture_output=True, text=True,
                              encoding="utf-8").stdout
with open(os.path.join(out_dir, "LICENSE"), "w", encoding="utf-8", newline="\n") as fp:
    fp.write(license_text)

models = len({p[0].lower() for p in profiles})
print(f"{len(profiles)} profiles ({models} models) from {len(sources)} sources, skipped {skipped}, commit {commit[:10]}")
