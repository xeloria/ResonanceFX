#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Builds the engine for the host (Linux / WSL) with AddressSanitizer + UBSan and runs the
# effect-framework harness in engine/tests/host_test.cpp. Needs g++ (or clang++) and zlib.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/host-test"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
mapfile -t SOURCES < <(find "$ROOT/engine/src" -name '*.cpp')
"$CXX" -std=c++20 -O1 -g -fno-omit-frame-pointer \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$ROOT/engine/src" -I"$ROOT/engine/src/core/include" -I"$ROOT/engine/tests" \
    -DRFX_VERSION_CODE=20000 -DRFX_VERSION_NAME='"test"' \
    "${SOURCES[@]}" "$ROOT/engine/tests/host_test.cpp" -o "$OUT/host_test" -lz -lpthread
"$OUT/host_test"
