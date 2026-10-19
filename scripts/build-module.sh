#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Packages the flashable module: out/ResonanceFX-<version>.zip
# Run scripts/build-engine.sh first.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^version=//p' "$ROOT/module/module.prop")"
STAGE="$ROOT/build/module"
ZIP="$ROOT/out/ResonanceFX-$VERSION.zip"

rm -rf "$STAGE"
mkdir -p "$STAGE/engine"
cp -r "$ROOT/module/." "$STAGE/"
cp "$ROOT/LICENSE" "$STAGE/LICENSE"
cp "$ROOT/NOTICE" "$STAGE/NOTICE"
for abi in arm64-v8a armeabi-v7a x86_64 x86; do
    lib="$ROOT/out/engine/$abi/libresonancefx.so"
    [ -f "$lib" ] || { echo "missing $lib - run scripts/build-engine.sh" >&2; exit 1; }
    mkdir -p "$STAGE/engine/$abi"
    cp "$lib" "$STAGE/engine/$abi/"
done

# Android shells choke on CRLF; make sure nothing slipped in from a Windows checkout.
find "$STAGE" -name '*.sh' -o -name '*.prop' -o -name 'update-binary' -o -name 'updater-script' | \
    while read -r f; do sed -i 's/\r$//' "$f"; done

rm -f "$ZIP"
python - "$STAGE" "$ZIP" <<'EOF'
import os, sys, zipfile
stage, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(stage):
        for name in sorted(files):
            path = os.path.join(base, name)
            arc = os.path.relpath(path, stage).replace(os.sep, "/")
            info = zipfile.ZipInfo(arc, date_time=(2026, 1, 1, 0, 0, 0))
            info.external_attr = (0o755 if name.endswith((".sh", "update-binary")) else 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as f:
                z.writestr(info, f.read())
EOF
echo "module -> ${ZIP#$ROOT/}"
