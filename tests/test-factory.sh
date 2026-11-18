#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Checks the effect-factory detection and the two entry shapes it selects:
#   - legacy (HIDL, or an AIDL HAL that kept native effect libraries): AELI library, no type=;
#   - AIDL: AIDL library, with the type="..." attribute the AIDL config parser needs.
# Also checks that switching a config from one shape to the other leaves exactly one entry.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

export RFX_DATA="$SANDBOX/data" MODDIR="$ROOT/module" RFX_LOGFILE=/dev/null
SHELL_CMD="${1:-bash}"

fail=0
check() { # check <name> <expected> <actual>
    if [ "$2" = "$3" ]; then echo "PASS  $1"; else echo "FAIL  $1: expected '$2', got '$3'"; fail=1; fi
}

run() { # run <shell statement>
    $SHELL_CMD -c '. "$MODDIR/common.sh"; eval "$1"' _ "$1"
}

UUID=4daa8eaa-0842-4a8d-82e9-6eee5827fbe4

# ---- factory detection ----------------------------------------------------------------------

maps() { # maps <file> <line>...
    f="$1"; shift
    : > "$f"
    for l in "$@"; do echo "$l" >> "$f"; done
}

legacy_maps="$SANDBOX/maps-legacy"
maps "$legacy_maps" \
    "7f00-7f10 r-xp 00000000 fd:00 1 /vendor/lib64/soundfx/libbundlewrapper.so" \
    "7f20-7f30 r-xp 00000000 fd:00 1 /vendor/lib64/soundfx/libresonancefx.so"
aidl_maps="$SANDBOX/maps-aidl"
maps "$aidl_maps" \
    "7f00-7f10 r-xp 00000000 fd:00 1 /apex/com.android.hardware.audio/lib64/soundfx/libequalizersw.so" \
    "7f20-7f30 r-xp 00000000 fd:00 1 /system/lib64/soundfx/libbassboostaidl.so"
mixed_maps="$SANDBOX/maps-mixed"
maps "$mixed_maps" \
    "7f00-7f10 r-xp 00000000 fd:00 1 /vendor/lib64/soundfx/libbundlewrapper.so" \
    "7f20-7f30 r-xp 00000000 fd:00 1 /system/lib64/soundfx/libbassboostaidl.so"
empty_maps="$SANDBOX/maps-empty"
: > "$empty_maps"

check "maps: legacy" legacy "$(run "rfx_classify_maps '$legacy_maps'")"
check "maps: aidl" aidl "$(run "rfx_classify_maps '$aidl_maps'")"
check "maps: both prefer aidl" aidl "$(run "rfx_classify_maps '$mixed_maps'")"
check "maps: empty" unknown "$(run "rfx_classify_maps '$empty_maps'")"

# ---- AIDL engine selection ------------------------------------------------------------------

mkdir -p "$SANDBOX/mod/engine/arm64-v8a"
: > "$SANDBOX/mod/engine/arm64-v8a/libresonancefx.so"
: > "$SANDBOX/mod/engine/arm64-v8a/libresonancefx_aidl.so"
: > "$SANDBOX/mod/engine/arm64-v8a/libresonancefx_aidl_36.so"
selector="rfx_lib_targets() { echo 'lib64 arm64-v8a'; }; rfx_api() { echo \$RFX_TEST_API; }; rfx_aidl_lib"
check "aidl lib: versioned build" libresonancefx_aidl_36.so \
    "$(run "MODDIR='$SANDBOX/mod'; RFX_TEST_API=36; $selector")"
check "aidl lib: unversioned fallback" libresonancefx_aidl.so \
    "$(run "MODDIR='$SANDBOX/mod'; RFX_TEST_API=35; $selector")"
# Android 17 (API 37) with only the API 36 build: the frozen AIDL interface still loads it.
# This used to return the legacy engine because the exact-API name did not exist.
check "aidl lib: older build on a newer platform" libresonancefx_aidl_36.so \
    "$(run "MODDIR='$SANDBOX/mod'; RFX_TEST_API=37; $selector")"

mkdir -p "$SANDBOX/mod-multi/engine/arm64-v8a"
: > "$SANDBOX/mod-multi/engine/arm64-v8a/libresonancefx_aidl_35.so"
: > "$SANDBOX/mod-multi/engine/arm64-v8a/libresonancefx_aidl_36.so"
check "aidl lib: newest build not newer than the platform" libresonancefx_aidl_36.so \
    "$(run "MODDIR='$SANDBOX/mod-multi'; RFX_TEST_API=37; $selector")"
check "aidl lib: exact match wins" libresonancefx_aidl_36.so \
    "$(run "MODDIR='$SANDBOX/mod-multi'; RFX_TEST_API=36; $selector")"
check "aidl lib: too-new build ignored" "" \
    "$(run "MODDIR='$SANDBOX/mod-multi'; RFX_TEST_API=34; $selector")"

# ---- entry shape ----------------------------------------------------------------------------

src="$ROOT/tests/fixtures/aosp_aidl_audio_effects_config.xml"
cfg="$SANDBOX/cfg.xml"
run "rfx_patch_any '$src' '$cfg' resonancefx_aidl libresonancefx_aidl.so $UUID" 2>/dev/null
check "aidl: library" 1 "$(grep -c 'name="resonancefx_aidl" path="libresonancefx_aidl.so"' "$cfg")"
check "aidl: type attribute" 1 "$(grep -c "library=\"resonancefx_aidl\" uuid=\"$UUID\" type=\"" "$cfg")"

legacy="$SANDBOX/legacy.xml"
run "rfx_patch_any '$src' '$legacy'" 2>/dev/null
check "legacy: library" 1 "$(grep -c 'name="resonancefx" path="libresonancefx.so"' "$legacy")"
check "legacy: no type attribute" 0 "$(grep -c 'name="resonancefx" .* type=' "$legacy")"

# Switching a config from the legacy shape to the AIDL shape must not leave both behind.
switched="$SANDBOX/switched.xml"
run "rfx_patch_any '$src' '$switched'; rfx_patch_any '$switched' '$switched' resonancefx_aidl libresonancefx_aidl.so $UUID" 2>/dev/null
check "switch: one library" 1 "$(grep -c '<library name="resonancefx' "$switched")"
check "switch: one effect" 1 "$(grep -c "uuid=\"$UUID\"" "$switched")"
check "switch: aidl library kept" 1 "$(grep -c 'libresonancefx_aidl.so' "$switched")"

exit $fail
