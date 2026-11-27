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

# Both builds must be in /system only: audioserver (system linker namespace) cannot dlopen
# /vendor/lib64/soundfx, and its .conf loader picks /vendor first if a copy exists (#17).
check "aidl install partition" /system \
    "$(run "rfx_api() { echo 37; }; rfx_lib_partitions_for libresonancefx_aidl_36.so")"
check "legacy install partition" /system \
    "$(run "rfx_api() { echo 37; }; rfx_lib_partitions_for libresonancefx.so")"

# The legacy partition is evidence, not a fixed choice: it is where the factory's *stock*
# plug-ins are mapped. Samsung's audioserver maps /system (#17); a Motorola/MTK factory maps
# /vendor (#20). One rule serves both, and no device name appears in it.
mkdir -p "$SANDBOX/data/state"
stock() { echo "rfx_stock_soundfx_path() { case \"\$1\" in $1 esac; }"; }
pexe() { echo "rfx_pid_exe() { case \"\$1\" in $1 esac; }"; }
check "legacy partition: default is system" /system \
    "$(run "rm -f \"\$RFX_STATE/legacy_partition\"; rfx_lib_partitions")"
check "legacy partition: stored choice is kept" /vendor \
    "$(run "echo /vendor > \"\$RFX_STATE/legacy_partition\"; rfx_lib_partitions")"
check "legacy detect: system audioserver beats a vendor HAL" /system \
    "$(run "rfx_effect_loader_pids() { echo '11 12'; }; $(stock '11) echo /system/lib64/soundfx/libbundlewrapper.so ;; 12) echo /vendor/lib64/soundfx/libaudiopreprocessing.so ;;'); $(pexe '11) echo /system/bin/audioserver ;; 12) echo /vendor/bin/hw/android.hardware.audio.service ;;'); rfx_detect_legacy_partition")"
check "legacy detect: audioserver mapping vendor plug-ins (MTK)" /vendor \
    "$(run "rfx_effect_loader_pids() { echo 11; }; $(stock '11) echo /vendor/lib64/soundfx/libbundlewrapper.so ;;'); $(pexe '11) echo /system/bin/audioserver ;;'); rfx_detect_legacy_partition")"
check "legacy detect: vendor-only loader" /vendor \
    "$(run "rfx_effect_loader_pids() { echo 12; }; $(stock '12) echo /vendor/lib64/soundfx/libaudiopreprocessing.so ;;'); $(pexe '12) echo /vendor/bin/hw/audiohalservice ;;'); rfx_detect_legacy_partition")"
check "legacy detect: nothing mapped is not a guess" "" \
    "$(run "rfx_effect_loader_pids() { echo 11; }; rfx_stock_soundfx_path() { :; }; rfx_detect_legacy_partition")"
check "detect_partitions: reports and stores a change" 0 \
    "$(run "rm -f \"\$RFX_STATE/legacy_partition\" \"\$RFX_STATE/aidl_partition\"; rfx_effect_loader_pids() { echo 12; }; $(stock '12) echo /vendor/lib64/soundfx/libaudiopreprocessing.so ;;'); $(pexe '12) echo /vendor/bin/hw/audiohalservice ;;'); rfx_detect_partitions && echo 0 || echo 1")"
check "detect_partitions: legacy stored" /vendor \
    "$(run "cat \"\$RFX_STATE/legacy_partition\"")"
check "detect_partitions: stable on the second run" 1 \
    "$(run "rfx_effect_loader_pids() { echo 12; }; $(stock '12) echo /vendor/lib64/soundfx/libaudiopreprocessing.so ;;'); $(pexe '12) echo /vendor/bin/hw/audiohalservice ;;'); rfx_detect_partitions && echo 0 || echo 1")"

stage="$SANDBOX/stage"
mkdir -p "$stage/engine/arm64-v8a"
: > "$stage/engine/arm64-v8a/libresonancefx.so"
: > "$stage/engine/arm64-v8a/libresonancefx_aidl_36.so"
# Staging follows the stored placement; start from the AOSP default for this case.
rm -f "$SANDBOX/data/state/legacy_partition" "$SANDBOX/data/state/aidl_partition"
run "MODDIR='$stage'; rfx_lib_targets() { echo 'lib64 arm64-v8a'; }; rfx_api() { echo 37; }; rfx_stage_libraries" 2>/dev/null
check "stage: legacy goes to system" 1 \
    "$([ -f "$stage/system/lib64/soundfx/libresonancefx.so" ] && echo 1 || echo 0)"
check "stage: legacy not in vendor" 0 \
    "$([ -f "$stage/system/vendor/lib64/soundfx/libresonancefx.so" ] && echo 1 || echo 0)"
check "stage: AIDL goes to system" 1 \
    "$([ -f "$stage/system/lib64/soundfx/libresonancefx_aidl_36.so" ] && echo 1 || echo 0)"
check "stage: AIDL not in vendor" 0 \
    "$([ -f "$stage/system/vendor/lib64/soundfx/libresonancefx_aidl_36.so" ] && echo 1 || echo 0)"

# A factory that loads its stock plug-ins from /vendor gets the engine there instead (#20), and
# the previous /system copy is removed: two copies would let checkLibraryPath() pick the wrong one.
run "echo /vendor > \"\$RFX_STATE/legacy_partition\"; echo /vendor > \"\$RFX_STATE/aidl_partition\"; MODDIR='$stage'; rfx_lib_targets() { echo 'lib64 arm64-v8a'; }; rfx_api() { echo 37; }; rfx_stage_libraries" 2>/dev/null
check "stage: vendor choice goes to vendor" 1 \
    "$([ -f "$stage/system/vendor/lib64/soundfx/libresonancefx.so" ] && echo 1 || echo 0)"
check "stage: vendor choice leaves no system copy" 0 \
    "$([ -f "$stage/system/lib64/soundfx/libresonancefx.so" ] && echo 1 || echo 0)"
check "stage: vendor choice AIDL too" 1 \
    "$([ -f "$stage/system/vendor/lib64/soundfx/libresonancefx_aidl_36.so" ] && echo 1 || echo 0)"
rm -f "$SANDBOX/data/state/legacy_partition" "$SANDBOX/data/state/aidl_partition"

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

# ---- effective configuration ----------------------------------------------------------------
# The framework reads the first readable audio_effects.xml in its search order. A ROM that ships
# only /system/etc must still get the effect (#11); one with a vendor/SKU copy must not get a
# second definition in /system (Samsung's audioserver would log "library not found" for it).

mkdir -p "$SANDBOX/eff/odm/etc" "$SANDBOX/eff/vendor/etc/audio/sku_pineapple" \
    "$SANDBOX/eff/vendor/etc" "$SANDBOX/eff/system/etc"
: > "$SANDBOX/eff/system/etc/audio_effects.xml"

search="rfx_config_search_dirs() { echo '$SANDBOX/eff/odm/etc'; echo '$SANDBOX/eff/vendor/etc/audio/sku_pineapple'; echo '$SANDBOX/eff/vendor/etc'; echo '$SANDBOX/eff/system/etc'; }"
effective() { # effective <search-prefix> <path>
    run "$1; rfx_is_effective_config '$2' && echo 0 || echo 1"
}
wanted() { # wanted <api> <is-effective-result> <path>
    run "rfx_api() { echo $1; }; rfx_is_effective_config() { return $2; }; rfx_config_wanted '$3' && echo 0 || echo 1"
}
check "config: system copy is effective without a vendor copy" 0 \
    "$(effective "$search" "$SANDBOX/eff/system/etc/audio_effects.xml")"
check "config: no higher-priority config wins" 1 \
    "$(effective "$search" "$SANDBOX/eff/vendor/etc/audio_effects.xml")"

: > "$SANDBOX/eff/vendor/etc/audio/sku_pineapple/audio_effects.xml"
check "config: SKU copy is effective" 0 \
    "$(effective "$search" "$SANDBOX/eff/vendor/etc/audio/sku_pineapple/audio_effects.xml")"
check "config: system copy hidden by the SKU copy" 1 \
    "$(effective "$search" "$SANDBOX/eff/system/etc/audio_effects.xml")"
# With ro.boot.product.vendor.sku unset the SKU directory is not searched at all.
nosku="rfx_config_search_dirs() { echo '$SANDBOX/eff/odm/etc'; echo '$SANDBOX/eff/vendor/etc'; echo '$SANDBOX/eff/system/etc'; }"
check "config: SKU copy is not effective without the property" 1 \
    "$(effective "$nosku" "$SANDBOX/eff/vendor/etc/audio/sku_pineapple/audio_effects.xml")"
check "config: system copy is effective when the SKU is not searched" 0 \
    "$(effective "$nosku" "$SANDBOX/eff/system/etc/audio_effects.xml")"

check "wanted: system config patched when it is the one read" 0 \
    "$(wanted 37 0 /system/etc/audio_effects.xml)"
check "wanted: system config skipped when a vendor copy wins" 1 \
    "$(wanted 37 1 /system/etc/audio_effects.xml)"
check "wanted: pre-Android 8 always patches system" 0 \
    "$(wanted 25 1 /system/etc/audio_effects.xml)"

# The Action runs after mounting, where a vendor/SKU config can be hidden by a namespace. The
# decision must come from the pre-mount snapshot, not from that view.
mkdir -p "$SANDBOX/data/state"
printf '/vendor/etc/audio_effects.xml|snap\n/system/etc/audio_effects.xml|snap2\n' > "$SANDBOX/data/state/config_sources"
check "wanted: snapshot vendor copy hides system" 1 \
    "$(run "rfx_api() { echo 37; }; rfx_config_wanted /system/etc/audio_effects.xml && echo 0 || echo 1")"
printf '/system/etc/audio_effects.xml|snap\n' > "$SANDBOX/data/state/config_sources"
check "wanted: snapshot with only system keeps it" 0 \
    "$(run "rfx_api() { echo 37; }; rfx_config_wanted /system/etc/audio_effects.xml && echo 0 || echo 1")"
: > "$SANDBOX/data/state/config_sources"

# ---- regeneration after mounting ------------------------------------------------------------
# rfx_generate_configs deletes the generated copies before rebuilding them. After the module is
# mounted the stock file can be hidden by that deletion, so re-finding comes up empty and the
# factory loses the effect (#11). Regeneration must use the pre-mount snapshot.

act="$SANDBOX/act"
mkdir -p "$act/mod" "$act/stock/vendor/etc/audio"
cat > "$act/stock/vendor/etc/audio/audio_effects.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<audio_effects_conf version="2.0" xmlns="http://schemas.android.com/audio/audio_effects_conf/v2_0">
    <libraries>
        <library name="bundle" path="libbundlewrapper.so"/>
    </libraries>
    <effects>
        <effect name="bassboost" library="bundle" uuid="8631f300-72e2-11df-b57e-0002a5d5c51b"/>
    </effects>
</audio_effects_conf>
XML

setup="MODDIR='$act/mod'; rfx_overlay_path() { echo '$act/mod/out/audio_effects.xml'; }; rfx_factory_mode() { echo legacy; }; rfx_aidl_lib() { :; };"
run "$setup rfx_find_configs() { echo '$act/stock/vendor/etc/audio/audio_effects.xml'; }; rfx_snapshot_configs; rfx_generate_configs"
check "generate: patched overlay written" 1 "$(grep -c "$UUID" "$act/mod/out/audio_effects.xml")"
snap="$SANDBOX/data/stage/src/$(echo "$act/stock/vendor/etc/audio/audio_effects.xml" | sed 's|/|__|g')"
check "generate: stock snapshot kept" 1 "$([ -s "$snap" ] && echo 1 || echo 0)"

# The Action case: the generated copy is gone and the live config is no longer discoverable.
run "$setup rm -f '$act/mod/out/audio_effects.xml'; rfx_find_configs() { :; }; rfx_generate_configs"
check "generate after mount: rebuilt from the snapshot" 1 "$(grep -c "$UUID" "$act/mod/out/audio_effects.xml")"
check "generate after mount: recorded again" 1 \
    "$(grep -c "^$act/stock/vendor/etc/audio/audio_effects.xml|" "$SANDBOX/data/state/configs")"

exit $fail
