#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Builds engine/tests/measure.cpp against the engine (Linux / WSL), writes build/measure.jsonl,
# prints a readable summary (tests/measure-report.py) and refreshes the measurements the app
# ships (scripts/gen-measurements.py).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/measure"
mkdir -p "$OUT"
CXX="${CXX:-g++}"

mapfile -t SOURCES < <(find "$ROOT/engine/src" -name '*.cpp')
"$CXX" -std=c++20 -O2 -I"$ROOT/engine/src" -I"$ROOT/engine/src/core/include" \
    -DRFX_VERSION_CODE=20000 -DRFX_VERSION_NAME='"measure"' \
    "${SOURCES[@]}" "$ROOT/engine/tests/measure.cpp" -o "$OUT/measure" -lpthread
"$OUT/measure" 2>/dev/null > "$ROOT/build/measure.jsonl"
python3 "$ROOT/tests/measure-report.py" "$ROOT/build/measure.jsonl"
# Refresh the app's "Engine measurements" page.
python3 "$ROOT/scripts/gen-measurements.py" "$ROOT/build/measure.jsonl"
