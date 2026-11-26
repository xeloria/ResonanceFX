# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# ResonanceFX shared module logic. Sourced by customize.sh, post-fs-data.sh, post-mount.sh,
# service.sh, action.sh and uninstall.sh. POSIX sh only (runs under Magisk/KernelSU/APatch
# busybox ash and under toybox mksh).
#
# Callers must set MODDIR (the installed module directory, or $MODPATH during install).

RFX_ID=resonancefx
RFX_UUID=4daa8eaa-0842-4a8d-82e9-6eee5827fbe4
# The type UUID reported in the effect descriptor (the legacy build uses the same value).
RFX_TYPE_UUID=ec7178ec-e5e1-4432-a3f4-4657e6795210
RFX_LIB=libresonancefx.so
RFX_LIB_NAME=resonancefx
RFX_EFFECT_NAME=resonancefx_standard
# The same engine built with the AIDL effect entry points (createEffect/queryEffect/
# destroyEffect). Android 16 audio effect HALs use the AIDL factory, which cannot load the
# legacy audio_effect_library_t ABI above; shipping both covers every ROM.
RFX_LIB_AIDL=libresonancefx_aidl.so
RFX_LIB_AIDL_NAME=resonancefx_aidl
# Overridable only so the host-side tests can sandbox everything under a temp directory.
RFX_DATA="${RFX_DATA:-/data/adb/resonancefx}"
RFX_STAGE=$RFX_DATA/stage
RFX_LOGDIR=$RFX_DATA/logs
RFX_STATE=$RFX_DATA/state
# Stock copies of the effect configs, taken before the module is mounted. Patching from these
# instead of re-reading the live system keeps the Action button from destroying a working
# install (see rfx_snapshot_configs).
RFX_SOURCES=$RFX_STAGE/src

# Our own library/effect names. Entries with these names (or our UUID) are removed before ours
# is added, so re-patching an already patched config never duplicates it.
RFX_OWN_NAMES="$RFX_LIB_NAME|$RFX_EFFECT_NAME|$RFX_LIB_AIDL_NAME"

mkdir -p "$RFX_LOGDIR" "$RFX_STATE" 2>/dev/null

rfx_log() {
    # $RFX_LOGFILE is chosen by the calling stage.
    echo "[$(date '+%H:%M:%S' 2>/dev/null)] $*" >> "${RFX_LOGFILE:-$RFX_LOGDIR/misc.log}" 2>/dev/null
    [ -n "$RFX_VERBOSE" ] && echo "$*"
    true
}

rfx_print() {
    if command -v ui_print >/dev/null 2>&1; then ui_print "$*"; else echo "$*"; fi
    rfx_log "$*"
}

# ---- Environment ---------------------------------------------------------------------------

rfx_root_impl() {
    # KernelSU and APatch export these to module scripts; Magisk exports nothing at boot.
    if [ "$KSU" = "true" ]; then
        echo kernelsu
    elif [ "$APATCH" = "true" ]; then
        echo apatch
    else
        echo magisk
    fi
}

rfx_api() { getprop ro.build.version.sdk; }

# ---- Effect factory detection ---------------------------------------------------------------

# A running audio effect factory has exactly one family of effect libraries mapped:
#   - AIDL: the AOSP AIDL factory only loads AIDL effect plug-ins (createEffect/queryEffect/
#     destroyEffect, IEffect over FMQs);
#   - legacy: HIDL HALs and AIDL HALs that kept the native effect libraries open the old
#     audio_effect_library_t (AELI) ABI.
# Which one is mapped is what decides whether the engine must be the AELI or the AIDL build, and
# whether an entry needs the type="..." attribute the AIDL config parser requires.
RFX_AIDL_LIBS='lib(bassboostaidl|bundleaidl|downmixaidl|dynamicsprocessingaidl|equalizersw|nssw|erasersw|hapticgeneratoraidl|loudnessenhanceraidl|preprocessingaidl|presetreverbsw|reverbaidl|virtualizersw|visualizeraidl|volumesw|aecsw|agc1sw|agc2sw|env_reverbsw|spatializersw|extensioneffect|resonancefx_aidl)\.so'
RFX_LEGACY_LIBS='lib(bundlewrapper|reverbwrapper|visualizer|downmix|dynproc|audiopreprocessing|hapticgenerator|ldnhncr|effectproxy|qcomvisualizer|qcomvoiceprocessing|qcompostprocbundle|volumelistener|asphere|mysound|myspace|audiosaplus_sec|swdap|v4a_re|jamesdsp|resonancefx)\.so'
# Same list without our own legacy entry: seeing our own name in a config only says that an
# earlier run patched it, not that the factory can load the legacy ABI.
RFX_LEGACY_CFG_LIBS='lib(bundlewrapper|reverbwrapper|visualizer|downmix|dynproc|audiopreprocessing|hapticgenerator|ldnhncr|effectproxy|qcomvisualizer|qcomvoiceprocessing|qcompostprocbundle|volumelistener|asphere|mysound|myspace|audiosaplus_sec|swdap|v4a_re|jamesdsp)\.so'

# rfx_classify_maps <maps file>...: prints "aidl", "legacy" or "unknown" for what those
# processes have open under .../soundfx. AIDL wins when both families are present.
rfx_classify_maps() {
    for f in "$@"; do
        [ -r "$f" ] || continue
        grep -qE "$RFX_AIDL_LIBS" "$f" 2>/dev/null && { echo aidl; return; }
    done
    for f in "$@"; do
        [ -r "$f" ] || continue
        grep -qE "$RFX_LEGACY_LIBS" "$f" 2>/dev/null && { echo legacy; return; }
    done
    echo unknown
}

# Inspect the live system. Only meaningful while the audio HAL is running (install, Action,
# late boot); before that it returns "unknown" and the stored mode is used. Memoised because the
# /proc scan is not free and the answer does not change during one script run.
rfx_factory_mode_live() {
    [ -n "${RFX_MODE_LIVE:-}" ] && { echo "$RFX_MODE_LIVE"; return; }
    set --
    for maps in /proc/[0-9]*/maps; do
        [ -r "$maps" ] || continue
        grep -q '/soundfx/' "$maps" 2>/dev/null || continue
        set -- "$@" "$maps"
    done
    if [ "$#" -gt 0 ]; then
        m="$(rfx_classify_maps "$@")"
        case "$m" in
            aidl|legacy) RFX_MODE_LIVE=$m; echo "$m"; return ;;
        esac
    fi
    # Nothing mapped yet (first boot, or a factory whose config lists no plug-ins): the AIDL
    # config itself says which plug-in family the ROM installs. This also lets the module
    # bootstrap itself onto the AIDL build once it is packaged.
    for cfg in /odm/etc/audio_effects_config.xml /vendor/etc/audio_effects_config.xml /system/etc/audio_effects_config.xml; do
        [ -f "$cfg" ] || continue
        grep -qE "$RFX_AIDL_LIBS" "$cfg" 2>/dev/null && { RFX_MODE_LIVE=aidl; echo aidl; return; }
        grep -qE "$RFX_LEGACY_CFG_LIBS" "$cfg" 2>/dev/null && { RFX_MODE_LIVE=legacy; echo legacy; return; }
    done
    # Even with no plug-in loaded yet, the stock AOSP AIDL effect service running means the
    # legacy AELI ABI can never be registered, so do not offer it the legacy library.
    for exe in /proc/[0-9]*/exe; do
        t="$(readlink "$exe" 2>/dev/null)"
        case "$t" in */effect.service-aidl*) RFX_MODE_LIVE=aidl; echo aidl; return ;; esac
    done
    # "unknown" is not memoised: the factory may not be up yet, and a later call must retry.
    echo unknown
}

# The mode to use now: the live answer if we have one, else what was detected earlier.
rfx_factory_mode() {
    m="$(rfx_factory_mode_live)"
    if [ "$m" = unknown ] && [ -f "$RFX_STATE/factory" ]; then
        m="$(cat "$RFX_STATE/factory" 2>/dev/null)"
    fi
    case "$m" in aidl|legacy) echo "$m" ;; *) echo unknown ;; esac
}

rfx_record_factory_mode() {
    m="$(rfx_factory_mode_live)"
    [ "$m" = unknown ] || echo "$m" > "$RFX_STATE/factory"
}

# The AIDL factory reads audio_effects_config.xml; everything else is a HIDL/legacy file.
rfx_is_aidl_config() {
    case "${1##*/}" in
        audio_effects_config*.xml) return 0 ;;
    esac
    return 1
}

# The ABI whose engine is installed (the 64-bit one when the device has both).
rfx_primary_abi() { rfx_lib_targets | awk 'NR == 1 { print $2 }'; }

# The lib directory of the primary ABI (lib64 or lib).
rfx_primary_libdir() { rfx_lib_targets | awk 'NR == 1 { print $1 }'; }

# The AIDL engine packaged for this platform, if any. Builds are named after the Android
# release they were compiled against (<name>_<api>.so), but the AIDL interface library is
# frozen and stays on newer releases, so a build for an older API keeps working on a newer
# one (an API 36 build on Android 17). Order: exact API, then the newest build not newer
# than this platform, then the unversioned single-platform default.
rfx_aidl_lib() {
    abi="$(rfx_primary_abi)"
    dir="$MODDIR/engine/$abi"
    api="$(rfx_api)"
    case "$api" in ''|*[!0-9]*) api=0 ;; esac

    if [ -f "$dir/libresonancefx_aidl_$api.so" ]; then
        echo "libresonancefx_aidl_$api.so"
        return
    fi

    best=""
    best_api=0
    for f in "$dir"/libresonancefx_aidl_*.so; do
        [ -f "$f" ] || continue
        n="${f##*/}"
        n="${n#libresonancefx_aidl_}"
        n="${n%.so}"
        case "$n" in ''|*[!0-9]*) continue ;; esac
        [ "$n" -le "$api" ] || continue
        if [ "$n" -gt "$best_api" ]; then
            best_api="$n"
            best="${f##*/}"
        fi
    done
    if [ -n "$best" ]; then
        echo "$best"
        return
    fi

    [ -f "$dir/$RFX_LIB_AIDL" ] && echo "$RFX_LIB_AIDL"
}

# The engine build the current factory can load, or the legacy one when no AIDL build is shipped.
rfx_active_lib() {
    aidl="$(rfx_aidl_lib)"
    if [ "$(rfx_factory_mode)" = aidl ] && [ -n "$aidl" ]; then
        echo "$aidl"
    else
        echo "$RFX_LIB"
    fi
}

# ---- Effect process visibility --------------------------------------------------------------
#
# A bind mount made from a root shell is not necessarily visible to the process that loads
# effects (KernelSU Next runs module scripts in their own mount namespace, and vendors can give
# audio services a private one). Everything that claims "the engine is in place" must ask the
# effect process, not our own shell: /proc/<pid>/root/<path> resolves through that process's
# mounts.

# Processes that load audio effects: audioserver and every audio HAL service. Matched on the
# executable path (not the command line) so a shell whose arguments mention audio_effects is
# never mistaken for the factory.
rfx_effect_pids() {
    [ -n "${RFX_EFFECT_PIDS:-}" ] && { echo "$RFX_EFFECT_PIDS"; return; }
    # Empty is a valid result (audio not started yet). Without this flag every caller would
    # rescan all of /proc, which made the post-mount stage slow enough to trip the boot watchdog.
    [ "${RFX_EFFECT_SCANNED:-0}" = "1" ] && return
    _pids=
    for exe in /proc/[0-9]*/exe; do
        [ -L "$exe" ] || continue
        target="$(readlink "$exe" 2>/dev/null)"
        case "$target" in
            *audio*)
                pid="${exe#/proc/}"; pid="${pid%/exe}"
                _pids="$_pids $pid" ;;
        esac
    done
    RFX_EFFECT_PIDS="${_pids# }"
    RFX_EFFECT_SCANNED=1
    echo "$RFX_EFFECT_PIDS"
}

# Processes that actually load effect plug-ins: they have a stock effect library mapped under
# /soundfx/. audioserver often maps none, so "any audio process" can see a file that the factory
# cannot; that false positive hid the real mount problem in #11. Falls back to rfx_effect_pids
# when no stock plug-in is mapped yet (fresh boot, or a factory whose config lists only us).
rfx_effect_loader_pids() {
    [ -n "${RFX_LOADER_PIDS:-}" ] && { echo "$RFX_LOADER_PIDS"; return; }
    # Empty is valid (no plug-in mapped yet); memoise it too.
    [ "${RFX_LOADER_SCANNED:-0}" = "1" ] && return
    _pids=
    for maps in /proc/[0-9]*/maps; do
        [ -r "$maps" ] || continue
        grep -q '/soundfx/' "$maps" 2>/dev/null || continue
        grep -qE "$RFX_AIDL_LIBS|$RFX_LEGACY_LIBS" "$maps" 2>/dev/null || continue
        pid="${maps#/proc/}"; pid="${pid%/maps}"
        _pids="$_pids $pid"
    done
    RFX_LOADER_PIDS="${_pids# }"
    RFX_LOADER_SCANNED=1
    echo "$RFX_LOADER_PIDS"
}

# Pids whose view of the filesystem decides whether the engine can load: the plug-in loader
# when one is identifiable, every audio process otherwise.
rfx_visibility_pids() {
    _pids="$(rfx_effect_loader_pids)"
    [ -n "$_pids" ] || _pids="$(rfx_effect_pids)"
    echo "$_pids"
}

# rfx_factory_sees <absolute path>: true when an effect loader can reach the path.
rfx_factory_sees() {
    _pids="$(rfx_visibility_pids)"
    if [ -z "$_pids" ]; then [ -e "$1" ]; return; fi
    for pid in $_pids; do
        [ -e "/proc/$pid/root$1" ] && return 0
    done
    return 1
}

# rfx_factory_matches <path> <staged>: true when an effect loader sees exactly the staged bytes.
rfx_factory_matches() {
    _pids="$(rfx_visibility_pids)"
    if [ -z "$_pids" ]; then cmp -s "$2" "$1"; return; fi
    for pid in $_pids; do
        cmp -s "$2" "/proc/$pid/root$1" && return 0
    done
    return 1
}

# rfx_is_mount <path>: true when <path> is a mount point in the current namespace.
rfx_is_mount() {
    awk -v t="$1" '$2 == t { f = 1 } END { exit !f }' /proc/mounts 2>/dev/null
}

# Removes every stacked bind on a node. A single umount only drops the top layer; an update that
# left two layers would keep pointing at a deleted stage directory.
rfx_drain_mount() {
    n=0
    while [ "$n" -lt 4 ] && rfx_is_mount "$1"; do
        umount "$1" 2>/dev/null || break
        n=$((n + 1))
    done
}

# True in a recovery session (module flashing on a ROM, or a recovery-mode boot). Guarding the
# mount stages against this avoids binds that die with the session and mis-reported boot state.
rfx_in_recovery() {
    [ "$RECOVERYMODE" = true ] && return 0
    [ "$BOOTMODE" = true ] && return 1  # Magisk says we are in a normal boot
    [ -n "$(getprop ro.twrp.version 2>/dev/null)" ] && return 0
    [ -n "$(getprop ro.orangefox.version 2>/dev/null)" ] && return 0
    [ -n "$(getprop ro.skydragon.version 2>/dev/null)" ] && return 0
    [ "$(getprop ro.bootmode 2>/dev/null)" = "recovery" ] && return 0
    [ "$(getprop init.svc.recovery 2>/dev/null)" != "" ] && return 0
    return 1
}

# Space separated "<libdir> <abi>" pairs the device can load effect libraries from.
# A 64-bit device may still run a 32-bit audio HAL (common on Qualcomm), so the 32-bit
# library is installed whenever the device has any 32-bit userspace.
rfx_lib_targets() {
    abilist64="$(getprop ro.product.cpu.abilist64)"
    abilist32="$(getprop ro.product.cpu.abilist32)"
    [ -z "$abilist64$abilist32" ] && abilist32="$(getprop ro.product.cpu.abi)"
    case "$abilist64" in
        *arm64-v8a*) echo "lib64 arm64-v8a" ;;
        *x86_64*) echo "lib64 x86_64" ;;
    esac
    case "$abilist32" in
        *armeabi-v7a*) echo "lib armeabi-v7a" ;;
        *x86*) echo "lib x86" ;;
    esac
}

# Partitions the legacy engine is installed to: /system only. Both the vendor audio HAL and the
# system-namespace audioserver can dlopen /system/lib64/soundfx, but audioserver cannot load
# /vendor/lib64/soundfx — its linker namespace rejects it, and the .conf loader's
# checkLibraryPath() picks /vendor first. A vendor copy therefore must not exist (#17).
rfx_lib_partitions() {
    echo /system
    true
}

# Partition whose linker namespace can dlopen the AIDL build. A vendor-hosted effect HAL
# (its own process, e.g. /vendor/bin/hw/audiohalservice.*) only reaches /vendor; a factory
# hosted by audioserver or an APEX only reaches /system. The factory keeps the first copy it
# finds and does not fall back, so exactly one copy may exist and it has to be in the right
# place. The choice is detected once (install/action time, when the HAL is running) and kept
# in $RFX_STATE/aidl_partition for the early boot passes.
rfx_aidl_partition() {
    case "$(cat "$RFX_STATE/aidl_partition" 2>/dev/null)" in
        /vendor|/system) cat "$RFX_STATE/aidl_partition"; return 0 ;;
    esac
    echo /system
}

# Which partition hosts the effect factory? Only meaningful while the audio HAL is running.
# Vendor processes win over system ones: a ROM may run both, but only a vendor-hosted HAL
# needs the /vendor copy.
rfx_detect_aidl_partition() {
    _pids="$(rfx_effect_pids 2>/dev/null)"
    for _pid in $_pids; do
        case "$(readlink "/proc/$_pid/exe" 2>/dev/null)" in
            /vendor/*) echo /vendor; return 0 ;;
        esac
    done
    for _pid in $_pids; do
        case "$(readlink "/proc/$_pid/exe" 2>/dev/null)" in
            /system/*|/apex/*) echo /system; return 0 ;;
        esac
    done
    true
}

# Partitions a given engine build may be installed to.
rfx_lib_partitions_for() {
    case "$1" in
        libresonancefx_aidl*) rfx_aidl_partition ;;
        *) rfx_lib_partitions ;;
    esac
    true
}

# Directories the framework searches for audio_effects*.xml, in order (system/media
# audio_config.h). The SKU directory is only on the list when the bootloader set
# ro.boot.product.vendor.sku, so a ROM that ships only the SKU copy makes the factory fall
# through to the next candidate.
rfx_config_search_dirs() {
    echo /odm/etc
    sku="$(getprop ro.boot.product.vendor.sku 2>/dev/null)"
    [ -n "$sku" ] && echo "/vendor/etc/audio/sku_$sku"
    echo /vendor/etc
    echo /system/etc
    true
}

# The first existing config with this file name in the framework's search order.
rfx_first_config() {
    for d in $(rfx_config_search_dirs); do
        if [ -f "$d/$1" ]; then echo "$d/$1"; return; fi
    done
}

# The pre-Android 8 config the legacy factory reads, in its own order (libeffects falls back to
# /vendor/etc/audio_effects.conf, then /system/etc/audio_effects.conf, when no XML parses).
rfx_first_conf() {
    for f in /odm/etc/audio_effects.conf /vendor/etc/audio_effects.conf /system/etc/audio_effects.conf; do
        [ -f "$f" ] && { echo "$f"; return; }
    done
}

# True when the framework would parse this path: it is the first existing file with that name.
rfx_is_effective_config() {
    first="$(rfx_first_config "${1##*/}")"
    [ -n "$first" ] || return 1
    [ "$(readlink -f "$first" 2>/dev/null || echo "$first")" = "$1" ]
}

# True when the /system copy is the config the framework would parse. Decided from the boot
# snapshot (taken before the module is mounted), not from the action's shell, where a vendor or
# SKU config can be hidden by a mount namespace and /system then looks like the only copy.
rfx_system_config_effective() {
    name="${1##*/}"
    [ -f "$RFX_STATE/config_sources" ] || return 1
    while IFS='|' read -r src _; do
        case "$src" in
            /odm/*|/vendor/*)
                [ "${src##*/}" = "$name" ] && return 1
                ;;
        esac
    done < "$RFX_STATE/config_sources"
    return 0
}

# The engine itself lives in /system now, but the /system *config* is still only patched when it
# is the one the framework parses: a /system mount is visible to app-side native detectors and
# the audioserver config loader cannot use a vendor library from it. When a vendor/SKU config
# exists it already carries the effect.
rfx_config_wanted() {
    case "$1" in
        /system/vendor/*) return 0 ;;
        /system/*)
            if [ "$(rfx_api)" -lt 26 ]; then
                return 0
            fi
            if [ -f "$RFX_STATE/config_sources" ]; then
                rfx_system_config_effective "$1"
            else
                rfx_is_effective_config "$1"
            fi
            return
            ;;
        *) return 0 ;;
    esac
}

# Every audio effects configuration the platform or a vendor HAL might read. All of them get
# the effect: which files a HAL loads is vendor-specific (Samsung's HIDL HAL reads
# audio_effects_haptic/_spatializer/_sec.xml but not audio_effects.xml), and a definition
# repeated in several loaded files is merely skipped with an "already exist" warning.
# Sources: the HIDL/legacy
# audio_effects.xml, the AIDL audio_effects_config.xml, SKU variants under etc/audio/sku_*,
# vendor-specific names (audio_effects_sec.xml, ...) and pre-O audio_effects.conf.
rfx_find_configs() {
    for dir in /odm/etc /vendor/etc /vendor/odm/etc /system/etc /system_ext/etc /product/etc \
               /my_product/etc /mi_ext/etc /system/vendor/etc; do
        [ -d "$dir" ] || continue
        find "$dir" -maxdepth 4 -type f \
            \( -name 'audio_effects*.xml' -o -name 'audio_effects*.conf' \) 2>/dev/null
    done | while read -r f; do
        real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
        echo "$real"
    done | sort -u
}

# Module overlay path for a real path, or empty if the root implementation can't overlay it.
rfx_overlay_path() {
    case "$1" in
        /system/vendor/*) echo "$MODDIR/system/vendor/${1#/system/vendor/}" ;;
        /vendor/*) echo "$MODDIR/system/vendor/${1#/vendor/}" ;;
        /system_ext/*) echo "$MODDIR/system/system_ext/${1#/system_ext/}" ;;
        /product/*) echo "$MODDIR/system/product/${1#/product/}" ;;
        /system/*) echo "$MODDIR/$1" ;;
        *) echo "" ;;
    esac
}

rfx_context_of() {
    ctx="$(ls -Zd "$1" 2>/dev/null | awk '{ for (i = 1; i <= NF; i++) if ($i ~ /^u:object_r:/) { print $i; exit } }')"
    if [ -n "$ctx" ]; then echo "$ctx"; return; fi
    case "$1" in
        /vendor/*|/odm/*|/system/vendor/*) echo "u:object_r:vendor_configs_file:s0" ;;
        *) echo "u:object_r:system_file:s0" ;;
    esac
}

rfx_lib_context() {
    case "$1" in
        /vendor/*|/odm/*|/system/vendor/*) echo "u:object_r:vendor_file:s0" ;;
        *) echo "u:object_r:system_lib_file:s0" ;;
    esac
}

# ---- Config patching ------------------------------------------------------------------------

# rfx_patch_xml <src> <dst> [library name] [library path] [type uuid]: writes a copy of src
# with exactly one ResonanceFX library and effect entry. The optional arguments select the engine
# build and add the type="..." attribute the AIDL config parser needs; without them the legacy
# build is used. Returns 1 (and writes nothing) if src has no <libraries>/<effects> sections.
rfx_patch_xml() {
    src="$1"; dst="$2"
    libname="${3:-$RFX_LIB_NAME}"
    libpath="${4:-$RFX_LIB}"
    typ="${5:+ type=\"$5\"}"
    grep -q '<libraries' "$src" 2>/dev/null || return 1
    grep -q '<effects' "$src" 2>/dev/null || return 1
    mkdir -p "$(dirname "$dst")"
    awk -v names="$RFX_OWN_NAMES" -v uuid="$RFX_UUID" \
        -v lib="        <library name=\"$libname\" path=\"$libpath\"/>" \
        -v eff="        <effect name=\"$RFX_EFFECT_NAME\" library=\"$libname\" uuid=\"$RFX_UUID\"$typ/>" '
        # Whatever followed the anchor tag goes on its own line, so our entry always stays a
        # standalone line that the next run can recognise and replace.
        function tail(rest) { return (rest ~ /^[ \t]*$/) ? rest : "\n        " rest }
        BEGIN { doneLib = 0; doneEff = 0; inComment = 0 }
        {
            line = $0
            low = tolower(line)
            # Track comments so commented-out <libraries> tags are never used as anchors.
            startsComment = index(line, "<!--") > 0
            endsComment = index(line, "-->") > 0
            if (inComment) {
                if (endsComment) inComment = 0
                print line; next
            }
            if (startsComment && !endsComment) { inComment = 1; print line; next }
            if (startsComment && endsComment) { print line; next }

            # Drop any single-line element that references a legacy engine name or our UUID
            # (<library>, <effect>, <apply> in pre/postprocess, ...).
            if (low ~ /^[ \t]*<[a-z]+[ \t][^>]*\/>[ \t]*$/ &&
                (index(low, uuid) > 0 || low ~ ("\"(" names ")\""))) next

            if (!doneLib && match(line, /<libraries[ \t]*\/>/)) {
                line = substr(line, 1, RSTART - 1) "<libraries>\n" lib "\n    </libraries>" substr(line, RSTART + RLENGTH)
                doneLib = 1
            } else if (!doneLib && match(line, /<libraries[ \t]*>/)) {
                line = substr(line, 1, RSTART + RLENGTH - 1) "\n" lib tail(substr(line, RSTART + RLENGTH))
                doneLib = 1
            }
            if (!doneEff && match(line, /<effects[ \t]*\/>/)) {
                line = substr(line, 1, RSTART - 1) "<effects>\n" eff "\n    </effects>" substr(line, RSTART + RLENGTH)
                doneEff = 1
            } else if (!doneEff && match(line, /<effects[ \t]*>/)) {
                line = substr(line, 1, RSTART + RLENGTH - 1) "\n" eff tail(substr(line, RSTART + RLENGTH))
                doneEff = 1
            }
            print line
        }
        END { if (!doneLib || !doneEff) exit 3 }
    ' "$src" > "$dst.tmp" 2>/dev/null
    if [ $? -ne 0 ] || ! grep -q "$RFX_UUID" "$dst.tmp"; then
        rm -f "$dst.tmp"
        return 1
    fi
    if [ -f "$dst" ]; then
        # Preserve the inode: a live bind mount reads this file. `mv` would unlink it and the
        # HAL would then see a missing config (#17).
        if cat "$dst.tmp" > "$dst"; then rm -f "$dst.tmp"; else rm -f "$dst.tmp"; return 1; fi
    else
        mv -f "$dst.tmp" "$dst"
    fi
    return 0
}

# rfx_patch_conf <src> <dst>: legacy (pre-Android 8) audio_effects.conf. The library path is
# absolute here, so we reuse whatever soundfx directory the stock entries already point at.
rfx_patch_conf() {
    src="$1"; dst="$2"
    grep -q '^libraries {' "$src" 2>/dev/null || return 1
    grep -q '^effects {' "$src" 2>/dev/null || return 1
    libdir="$(grep -o '/[a-z0-9_/]*/soundfx' "$src" | head -n 1)"
    [ -z "$libdir" ] && libdir=/system/lib/soundfx
    mkdir -p "$(dirname "$dst")"
    awk -v names="$RFX_OWN_NAMES" -v uuid="$RFX_UUID" \
        -v libname="$RFX_LIB_NAME" -v effname="$RFX_EFFECT_NAME" -v libpath="$libdir/$RFX_LIB" '
        BEGIN { skip = 0; depth = 0; doneLib = 0; doneEff = 0 }
        {
            line = $0
            if (skip) {
                n = gsub(/\{/, "{", line); m = gsub(/\}/, "}", line)
                depth += n - m
                if (depth <= 0) skip = 0
                next
            }
            # Strip blocks named after a legacy engine library/effect.
            if (match(line, /^[ \t]*[A-Za-z0-9_]+[ \t]*\{[ \t]*$/)) {
                name = line; gsub(/[ \t{]/, "", name)
                if (name ~ ("^(" names ")$")) { skip = 1; depth = 1; next }
            }
            print line
            if (!doneLib && line ~ /^libraries \{/) {
                print "  " libname " {"; print "    path " libpath; print "  }"; doneLib = 1
            }
            if (!doneEff && line ~ /^effects \{/) {
                print "  " effname " {"; print "    library " libname; print "    uuid " uuid; print "  }"; doneEff = 1
            }
        }
        END { if (!doneLib || !doneEff) exit 3 }
    ' "$src" > "$dst.tmp" 2>/dev/null
    if [ $? -ne 0 ]; then
        rm -f "$dst.tmp"
        return 1
    fi
    if [ -f "$dst" ]; then
        # Preserve the inode: a live bind mount reads this file. `mv` would unlink it and the
        # HAL would then see a missing config (#17).
        if cat "$dst.tmp" > "$dst"; then rm -f "$dst.tmp"; else rm -f "$dst.tmp"; return 1; fi
    else
        mv -f "$dst.tmp" "$dst"
    fi
    return 0
}

rfx_patch_any() {
    case "$1" in
        *.conf) rfx_patch_conf "$1" "$2" ;;
        *) rfx_patch_xml "$1" "$2" "${3:-}" "${4:-}" "${5:-}" ;;
    esac
}

# ---- Module layout --------------------------------------------------------------------------

# Copies the right engine build into every soundfx directory the module overlays.
# Engine builds live in $MODDIR/engine/<abi>/libresonancefx.so (legacy AELI ABI) and, when the
# AIDL front-end is built, $MODDIR/engine/<abi>/libresonancefx_aidl*.so.
rfx_stage_libraries() {
    rfx_lib_targets | while read -r libdir abi; do
        srcLib="$MODDIR/engine/$abi/$RFX_LIB"
        if [ ! -f "$srcLib" ]; then
            rfx_log "missing engine build for $abi"
            continue
        fi
        # Remove every stale copy (including older module versions): both the AIDL front-end and
        # the legacy engine live in /system only. A /vendor legacy copy is chosen first by
        # audioserver's .conf loader and then rejected by its linker namespace (#17).
        for d in "$MODDIR/system/vendor/$libdir/soundfx" "$MODDIR/system/$libdir/soundfx"; do
            [ -d "$d" ] || continue
            rm -f "$d"/libresonancefx_aidl*.so
            rm -f "$d/$RFX_LIB"
        done

        for part in $(rfx_lib_partitions_for "$RFX_LIB"); do
            case "$part" in
                /vendor) dir="$MODDIR/system/vendor/$libdir/soundfx" ;;
                *) dir="$MODDIR/system/$libdir/soundfx" ;;
            esac
            mkdir -p "$dir"
            chmod 0755 "$dir"
            cp -f "$srcLib" "$dir/$RFX_LIB"
            chmod 0644 "$dir/$RFX_LIB"
            chcon "$(rfx_lib_context "$part/")" "$dir/$RFX_LIB" 2>/dev/null
            rfx_log "staged $abi -> $part/$libdir/soundfx/$RFX_LIB"
        done

        # The AIDL front-end goes where the same factory can reach it (see
        # rfx_lib_partitions_for).
        for src in "$MODDIR/engine/$abi"/libresonancefx_aidl*.so; do
            [ -f "$src" ] || continue
            name="${src##*/}"
            for part in $(rfx_lib_partitions_for "$name"); do
                case "$part" in
                    /vendor) dir="$MODDIR/system/vendor/$libdir/soundfx" ;;
                    *) dir="$MODDIR/system/$libdir/soundfx" ;;
                esac
                mkdir -p "$dir"
                chmod 0755 "$dir"
                cp -f "$src" "$dir/$name"
                chmod 0644 "$dir/$name"
                chcon "$(rfx_lib_context "$part/")" "$dir/$name" 2>/dev/null
                rfx_log "staged $abi -> $part/$libdir/soundfx/$name"
            done
        done
    done
}

# Snapshots every stock config before the module is mounted (post-fs-data and install). The
# Action button runs after mounting, where deleting an overlay copy can hide the stock file:
# re-finding the configs then comes up empty and the factory loses the effect. Patching from a
# pre-mount snapshot makes regeneration independent of that view.
# Records each target in $RFX_STATE/config_sources as "<real path>|<stock copy>".
rfx_snapshot_configs() {
    rm -rf "$RFX_SOURCES"
    mkdir -p "$RFX_SOURCES"
    : > "$RFX_STATE/config_sources"
    rfx_find_configs | while read -r cfg; do
        [ -f "$cfg" ] || continue
        snap="$RFX_SOURCES/$(echo "$cfg" | sed 's|/|__|g')"
        cp -f "$cfg" "$snap" 2>/dev/null || { rfx_log "cannot snapshot $cfg"; continue; }
        chmod 0644 "$snap"
        echo "$cfg|$snap" >> "$RFX_STATE/config_sources"
    done
}

# Regenerates every patched config from the stock snapshots. Run at install time and again
# early on every boot, so OTA updates and vendor config changes never leave a stale copy.
# Records each target in $RFX_STATE/configs as "<real path>|<patched copy>".
rfx_generate_configs() {
    # No snapshot (an Action run before any boot script, or an old install): take one now, from
    # whatever is visible, so the module can still try to do the right thing.
    [ -s "$RFX_STATE/config_sources" ] || rfx_snapshot_configs

    : > "$RFX_STATE/configs"
    # Drop previous generated copies so configs removed by an OTA don't linger in the overlay.
    # KernelSU moves system/vendor (and odm, product, system_ext) to the module root and leaves a
    # symlink behind, which find doesn't follow, so walk every partition directory explicitly.
    for d in system vendor odm product system_ext; do
        [ -d "$MODDIR/$d" ] && find "$MODDIR/$d/" -type f -name 'audio_effects*' -exec rm -f {} \; 2>/dev/null
    done
    # Keep $RFX_STAGE/cfg: those files are the source of live bind mounts. Deleting them makes
    # the HAL's mounts point at unlinked inodes (the config then "disappears", #17). Each target
    # is overwritten in place below.
    mkdir -p "$RFX_STAGE/cfg"

    # Which engine build and entry shape each config family needs. The AIDL factory cannot load
    # the legacy ABI, so only offer it an AIDL build; until one is packaged, leave its config
    # alone rather than list a library it will reject (a failing entry makes the factory drop its
    # whole effect list). Legacy files keep the AELI build, as before.
    mode="$(rfx_factory_mode)"
    aidl_lib="$(rfx_aidl_lib)"
    echo "$mode" > "$RFX_STATE/factory_used"

    while IFS='|' read -r cfg src; do
        [ -n "$cfg" ] || continue
        [ -f "$src" ] || { rfx_log "missing stock copy of $cfg"; continue; }
        rfx_config_wanted "$cfg" || { rfx_log "left alone $cfg (framework copy)"; continue; }
        libname="$RFX_LIB_NAME"; libpath="$RFX_LIB"; typ=""
        if rfx_is_aidl_config "$cfg"; then
            if [ "$mode" = aidl ]; then
                [ -n "$aidl_lib" ] || { rfx_log "left alone $cfg (AIDL effect factory, no AIDL engine)"; continue; }
                libname="$RFX_LIB_AIDL_NAME"; libpath="$aidl_lib"
            fi
            # The AIDL parser drops effects whose name is not a built-in type unless the entry
            # carries its own type UUID; legacy parsers ignore the attribute.
            [ "$mode" = unknown ] || typ="$RFX_TYPE_UUID"
        fi
        overlay="$(rfx_overlay_path "$cfg")"
        # Always keep a staged copy for the bind-mount fallback.
        staged="$RFX_STAGE/cfg$(echo "$cfg" | sed 's|/|__|g')"
        if rfx_patch_any "$src" "$staged" "$libname" "$libpath" "$typ"; then
            chmod 0644 "$staged"
            chcon "$(rfx_context_of "$cfg")" "$staged" 2>/dev/null
            if [ -n "$overlay" ]; then
                mkdir -p "$(dirname "$overlay")"
                cp -f "$staged" "$overlay"
                chmod 0644 "$overlay"
                chcon "$(rfx_context_of "$cfg")" "$overlay" 2>/dev/null
            fi
            echo "$cfg|$staged" >> "$RFX_STATE/configs"
            rfx_log "patched $cfg"
        else
            rfx_log "skipped $cfg (no libraries/effects sections)"
        fi
    done < "$RFX_STATE/config_sources"
}

# ---- Verification & fallback ----------------------------------------------------------------

# APEXes whose soundfx directory carries AIDL effect plug-ins. An APEX-hosted AIDL factory
# resolves a relative library name in its own /apex/<name>/<libdir>/soundfx first, and that path
# is permitted by its linker namespace even when /vendor is rejected. Not every APEX has a
# soundfx directory; the stock plug-in names are the marker.
rfx_aidl_apex_dirs() {
    apex="${RFX_APEX_ROOT:-/apex}"
    for d in "$apex"/*/lib64/soundfx "$apex"/*/lib/soundfx; do
        [ -d "$d" ] || continue
        ls "$d" 2>/dev/null | grep -qE "$RFX_AIDL_LIBS" || continue
        echo "$d"
    done | sort -u
}

# Every path the active build can legitimately live at, in the factory's search order.
rfx_active_lib_paths() {
    name="$(rfx_active_lib)"
    case "$name" in
        libresonancefx_aidl*)
            for d in $(rfx_aidl_apex_dirs); do echo "$d/$name"; done
            echo "/system/$(rfx_primary_libdir)/soundfx/$name"
            ;;
        *)
            for libdir in $(rfx_lib_targets | awk '{ print $1 }'); do
                for part in $(rfx_lib_partitions_for "$name"); do
                    echo "$part/$libdir/soundfx/$name"
                done
            done
            ;;
    esac
}

# Directories to bind the staged engine over for a given build and libdir. AIDL gets the APEX
# soundfx dirs (the APEX factory's first search path) plus /system; legacy gets /vendor.
rfx_lib_bind_targets() {
    active="$1"
    libdir="$2"
    case "$active" in
        libresonancefx_aidl*)
            for d in $(rfx_aidl_apex_dirs); do
                case "$d" in */"$libdir"/soundfx) echo "$d" ;; esac
            done
            echo "/system/$libdir/soundfx"
            ;;
        *)
            for part in $(rfx_lib_partitions_for "$active"); do
                echo "$part/$libdir/soundfx"
            done
            ;;
    esac
}

rfx_lib_visible() {
    # True when at least one candidate is visible to the process that loads effects.
    for f in $(rfx_active_lib_paths); do
        rfx_factory_sees "$f" && return 0
    done
    return 1
}

# Bind-mounts a soundfx directory that also contains our library over the real one. Used when
# the root implementation didn't overlay /vendor (e.g. KernelSU on some Samsung firmware).
rfx_bind_libraries() {
    active="$(rfx_active_lib)"
    primary="$(rfx_primary_libdir)"
    rfx_lib_targets | while read -r libdir abi; do
        case "$active" in
            libresonancefx_aidl*) [ "$libdir" = "$primary" ] || continue ;;
        esac
        for target in $(rfx_lib_bind_targets "$active" "$libdir"); do
            [ -z "${RFX_FORCE_BIND:-}" ] && rfx_factory_sees "$target/$active" && continue
            [ -d "$target" ] || continue
            lib="$MODDIR/engine/$abi/$active"
            if [ ! -f "$lib" ]; then
                rfx_log "missing engine build $lib; not binding $target"
                continue
            fi
            stage="$RFX_STAGE/$(echo "$target" | sed 's|/|_|g')"
            # Drop any previous mount before touching its source; deleting the upper/stage of a
            # live mount exposes missing files to the HAL.
            rfx_drain_mount "$target"
            rm -rf "$stage"
            mkdir -p "$stage/upper" "$stage/work"

            # Preferred: an overlay. The stock libraries stay in the read-only lower layer, so
            # there is no large copy at boot (copying /vendor/lib64/soundfx tripped the boot
            # watchdog on the S24 in #17).
            if cp -f "$lib" "$stage/upper/$active" 2>/dev/null; then
                chmod 0644 "$stage/upper/$active"
                chcon "$(rfx_lib_context "$target/")" "$stage/upper/$active" 2>/dev/null
                chcon "$(rfx_context_of "$target")" "$stage/upper" 2>/dev/null
                chmod 0755 "$stage" "$stage/upper"
                if mount -t overlay overlay \
                    -o "lowerdir=$target,upperdir=$stage/upper,workdir=$stage/work" "$target" 2>/dev/null; then
                    rfx_log "overlay-mounted library dir $target"
                    echo bind > "$RFX_STATE/lib_mode"
                    if rfx_factory_sees "$target/$active"; then
                        rfx_log "audio HAL sees $target/$active"
                    else
                        rfx_log "audio HAL CANNOT see $target/$active (mount not shared; reboot required)"
                    fi
                    continue
                fi
            fi

            # Fallback for kernels without overlayfs: copy the stock libraries and bind. Never
            # do this in post-mount (the copy can take minutes and trips the boot watchdog).
            if [ "${RFX_NO_COPY:-0}" = "1" ]; then
                rfx_log "overlay unavailable for $target; deferring the copy to the service stage"
                rm -rf "$stage"
                continue
            fi
            rm -rf "$stage"
            mkdir -p "$stage"
            copyfail=0
            for f in "$target"/*; do
                [ -f "$f" ] || continue
                cp -f "$f" "$stage/" || { copyfail=1; break; }
                chmod 0644 "$stage/${f##*/}"
                chcon "$(rfx_context_of "$f")" "$stage/${f##*/}" 2>/dev/null
            done
            if [ "$copyfail" = "1" ]; then
                rfx_log "not binding $target (could not copy its stock libraries)"
                rm -rf "$stage"
                continue
            fi
            cp -f "$lib" "$stage/$active" || {
                rfx_log "cannot copy $lib; not binding $target"
                rm -rf "$stage"
                continue
            }
            chmod 0644 "$stage/$active"
            chcon "$(rfx_lib_context "$target/")" "$stage/$active" 2>/dev/null
            chmod 0755 "$stage"
            chcon "$(rfx_context_of "$target")" "$stage" 2>/dev/null
            if mount --bind "$stage" "$target"; then
                rfx_log "bind-mounted library dir $target"
                echo bind > "$RFX_STATE/lib_mode"
                if rfx_factory_sees "$target/$active"; then
                    rfx_log "audio HAL sees $target/$active"
                else
                    rfx_log "audio HAL CANNOT see $target/$active (mount not shared; reboot required)"
                fi
            else
                rfx_log "FAILED to bind-mount $target"
            fi
        done
    done
}

# Bind-mounts each patched config the effect process doesn't see yet.
rfx_bind_configs() {
    [ -f "$RFX_STATE/configs" ] || return 0
    while IFS='|' read -r cfg staged; do
        [ -f "$staged" ] || continue
        rfx_factory_matches "$cfg" "$staged" && continue
        # Drop every layer left by a previous pass before mounting a fresh one (see
        # rfx_bind_libraries: stacked binds outlive their stage file).
        rfx_drain_mount "$cfg"
        if mount --bind "$staged" "$cfg"; then
            rfx_log "bind-mounted $cfg"
            echo bind > "$RFX_STATE/cfg_mode"
            rfx_factory_matches "$cfg" "$staged" ||
                rfx_log "audio HAL CANNOT see $cfg (mount not shared; reboot required)"
        else
            rfx_log "FAILED to bind-mount $cfg"
        fi
    done < "$RFX_STATE/configs"
}

rfx_configs_missing() {
    n=0
    [ -f "$RFX_STATE/configs" ] || { echo 0; return; }
    while IFS='|' read -r cfg staged; do
        rfx_factory_matches "$cfg" "$staged" 2>/dev/null || n=$((n + 1))
    done < "$RFX_STATE/configs"
    echo $n
}

# A bind-mounted soundfx dir can't be unmounted while the audio HAL has our library mapped
# (EBUSY), so an updated engine is swapped into the bind source instead: the mounted view
# changes immediately, the old inode stays valid for the running HAL until it restarts.
rfx_refresh_bound_libraries() {
    active="$(rfx_active_lib)"
    primary="$(rfx_primary_libdir)"
    rfx_lib_targets | while read -r libdir abi; do
        case "$active" in
            libresonancefx_aidl*) [ "$libdir" = "$primary" ] || continue ;;
        esac
        for target in $(rfx_lib_bind_targets "$active" "$libdir"); do
            stage="$RFX_STAGE/$(echo "$target" | sed 's|/|_|g')"
            lib="$MODDIR/engine/$abi/$active"
            [ -f "$lib" ] || continue
            # The library lives in upper/ for an overlay mount and in the stage root for the
            # copy+bind fallback.
            for f in "$stage/upper/$active" "$stage/$active"; do
                [ -f "$f" ] || continue
                cmp -s "$lib" "$f" && continue
                cp -f "$lib" "$f.new" &&
                    chmod 0644 "$f.new" &&
                    chcon "$(rfx_lib_context "$target/")" "$f.new" 2>/dev/null
                mv -f "$f.new" "$f" && rfx_log "refreshed engine in $target"
            done
        done
    done
}

rfx_ensure_mounted() {
    rfx_refresh_bound_libraries
    if ! rfx_lib_visible; then
        rfx_log "library not visible after module mount, applying bind fallback"
        # Force the bind: the per-target visibility check can see the shell's view while the
        # loader's namespace still does not, which is exactly the Samsung/APEX case.
        RFX_FORCE_BIND=1 rfx_bind_libraries
    fi
    if [ "$(rfx_configs_missing)" -gt 0 ]; then
        rfx_log "configs not visible after module mount, applying bind fallback"
        rfx_bind_configs
    fi
}

rfx_unmount_all() {
    # Two passes: if a bind ever got stacked (an old lower mount under a new one), a single
    # umount only removes the top layer and the stage dir of the lower one is deleted next.
    for _pass in 1 2; do
        if [ -f "$RFX_STATE/configs" ]; then
            while IFS='|' read -r cfg staged; do
                rfx_drain_mount "$cfg"
            done < "$RFX_STATE/configs"
        fi
        for pair in $(rfx_lib_targets | tr ' ' ':'); do
            libdir="${pair%%:*}"
            for part in /vendor /system; do rfx_drain_mount "$part/$libdir/soundfx"; done
        done
        for d in $(rfx_aidl_apex_dirs); do rfx_drain_mount "$d"; done
    done
}

# Restarts audioserver and every audio HAL so a late mount takes effect without a reboot.
rfx_restart_audio() {
    rfx_log "restarting audio services"
    # Restart through init (never kill -9) so services flagged "critical" aren't counted as
    # crashing. Covers audioserver plus every HIDL/AIDL audio HAL, whatever the vendor named it.
    getprop | sed -n 's/^\[init\.svc\.\([^]]*\)\].*/\1/p' | while read -r svc; do
        case "$svc" in
            audioserver|*audio*hal*|*hal*audio*|*audio-service*|*audio_service*|*effect*)
                setprop ctl.restart "$svc"
                rfx_log "restarted $svc"
                ;;
        esac
    done
    # The effect processes are new now; forget the cached pids and the "already scanned" flags.
    RFX_EFFECT_PIDS=
    RFX_LOADER_PIDS=
    RFX_EFFECT_SCANNED=0
    RFX_LOADER_SCANNED=0
}

# The definitive check: did the audio HAL's effect factory actually register our UUID?
# (audio_flinger dumps every effect library and descriptor it knows.)
rfx_engine_registered() {
    dumpsys media.audio_flinger 2>/dev/null | grep -qi "$RFX_UUID" && return 0
    # audio_flinger does not list device effects on every ROM (AIDL factories that are not
    # connected to a chain show an empty list), so a mapped library is proof enough.
    rfx_lib_visible
}

# ---- Diagnosis ------------------------------------------------------------------------------
#
# One ordered decision tree over the same facts the boot stage, the Action button and bug reports
# use, with a stable code for every failure. The module description, the Action output and the
# app's report all name that code, so a user can quote "E06" instead of sending logs. Codes are
# documented in DIAGNOSTICS.md.
#
#   OK   the factory registered the engine
#   E02  no engine build for this CPU
#   E03  module files not mounted / not visible to the audio HAL
#   E04  no config that the effect HAL reads
#   E05  a config was found but could not be patched
#   E06  the config the factory reads has no ResonanceFX entry
#   E07  AIDL effect HAL, but this build has no AIDL engine for its API
#   E08  entry shape does not match the factory generation
#   E09  the factory cannot find the engine library
#   E10  the factory cannot load the engine library (namespace/ABI/deps/symbols/SELinux)
#   E11  the factory rejected the patched config
#   E14  the factory has not registered the engine yet
#   E15  the effect HAL generation could not be determined

rfx_diag() { # code short detail fix
    code="$1"
    short="$(printf '%s' "$2" | tr '\n|' ' ')"
    detail="$(printf '%s' "$3" | tr '\n|' ' ')"
    fix="$(printf '%s' "$4" | tr '\n|' ' ')"
    printf '%s|%s|%s|%s\n' "$code" "$short" "$detail" "$fix" > "$RFX_STATE/diagnosis"
    rfx_log "diagnosis: $code - $short${detail:+ ($detail)}"
}

rfx_diag_get() { # <field 1..4>
    [ -f "$RFX_STATE/diagnosis" ] || return 1
    cut -d'|' -f"$1" "$RFX_STATE/diagnosis"
}

# The path the active build is expected at: the first candidate the factory can see, else the
# first candidate (the APEX soundfx directory for an APEX-hosted AIDL HAL).
rfx_expected_lib_path() {
    for f in $(rfx_active_lib_paths); do
        rfx_factory_sees "$f" && { echo "$f"; return; }
    done
    rfx_active_lib_paths | head -n 1
}

# Reads our entry out of a patched config as "<library>|<path>|<type>". Works on the XML of both
# factories and on the pre-Android 8 .conf.
rfx_config_entry() {
    cfg="$1"
    lib=""; path=""; typ=""
    case "$cfg" in
        *.conf)
            if grep -q "$RFX_UUID" "$cfg" 2>/dev/null; then
                lib="$RFX_LIB_NAME"
                path="$(awk -v n="$RFX_LIB_NAME" '
                    $1 == n && $2 == "{" { inlib = 1; next }
                    inlib && $1 == "path" { print $2; exit }
                    inlib && $1 == "}" { inlib = 0 }
                ' "$cfg" 2>/dev/null)"
            fi
            ;;
        *)
            line="$(grep "$RFX_UUID" "$cfg" 2>/dev/null | head -n 1)"
            lib="$(printf '%s\n' "$line" | sed -n 's/.*library="\([^"]*\)".*/\1/p')"
            typ="$(printf '%s\n' "$line" | sed -n 's/.*type="\([^"]*\)".*/\1/p')"
            if [ -n "$lib" ]; then
                path="$(grep "<library name=\"$lib\"" "$cfg" 2>/dev/null | head -n 1 |
                    sed -n 's/.*path="\([^"]*\)".*/\1/p')"
            fi
            ;;
    esac
    printf '%s|%s|%s' "$lib" "$path" "$typ"
}

# Resolves a config's library path the way the factory does: a relative name is looked up in
# /odm, /vendor and /system <libdir>/soundfx, as seen by an effect process (not our shell).
# A 64-bit device can run a 32-bit audio HAL (common on Qualcomm), so try every installed ABI.
rfx_resolve_library() {
    p="$1"
    case "$p" in
        /*)
            if rfx_factory_sees "$p"; then echo "$p"; return 0; fi
            return 1
            ;;
    esac
    # An APEX-hosted AIDL factory searches its own APEX soundfx directory first.
    if [ "$(rfx_factory_mode)" = aidl ]; then
        for d in $(rfx_aidl_apex_dirs); do
            cand="$d/$p"
            if rfx_factory_sees "$cand"; then echo "$cand"; return 0; fi
        done
    fi
    for libdir in $(rfx_lib_targets | awk '{ print $1 }'); do
        for part in /odm /vendor /system; do
            cand="$part/$libdir/soundfx/$p"
            if rfx_factory_sees "$cand"; then echo "$cand"; return 0; fi
        done
    done
    return 1
}

# "<bits> <machine>" from an ELF header, or nothing when od is unavailable.
rfx_elf_abi() {
    hdr="$(od -An -tx1 -N20 "$1" 2>/dev/null | tr -d ' \n')"
    [ "${#hdr}" -ge 40 ] || return 1
    cls="$(printf '%s' "$hdr" | cut -c9-10)"
    mach="$(printf '%s' "$hdr" | cut -c37-40)"
    case "$cls" in 01) bits=32 ;; 02) bits=64 ;; *) return 1 ;; esac
    case "$mach" in
        2800) echo "$bits arm" ;;
        b700) echo "$bits arm64" ;;
        3e00) echo "$bits x86_64" ;;
        0300) echo "$bits x86" ;;
        *) echo "$bits unknown-$mach" ;;
    esac
}

# The architecture a resolved path must have; derived from its lib64/lib directory so a 32-bit
# HAL on a 64-bit device is not compared against the 64-bit build.
rfx_expected_elf_abi_for() {
    case "$1" in
        */lib64/soundfx/*)
            case "$(getprop ro.product.cpu.abilist64)" in
                *arm64-v8a*) echo "64 arm64" ;;
                *x86_64*) echo "64 x86_64" ;;
            esac
            ;;
        */lib/soundfx/*)
            case "$(getprop ro.product.cpu.abilist32)" in
                *armeabi-v7a*) echo "32 arm" ;;
                *x86*) echo "32 x86" ;;
            esac
            ;;
    esac
}

# The most recent factory error that is about loading our library or parsing the config. The
# "Error querying effect descriptor" lines are app requests that arrive when the factory does not
# know the UUID yet; they are evidence for E14, not load errors.
rfx_factory_error_line() {
    {
        [ -s "$RFX_LOGDIR/logcat.txt" ] && cat "$RFX_LOGDIR/logcat.txt"
        logcat -d -b all 2>/dev/null
    } | grep -iE "resonancefx|$RFX_UUID|$RFX_LIB|audio_effects" 2>/dev/null |
        grep -iE "dlopen|namespace|cannot|failed|error|denied|not found|no such file|parse|invalid" |
        grep -viE "already exist|Error querying effect descriptor|could not create effect|AudioFlinger could not create effect|audit|avc:|scontext=|tcontext=|permissive=|ActivityManager|ActivityTaskManager|Freecess|Notification|PackageManager|WindowManager|SurfaceFlinger|InputDispatcher|Launcher|PACKAGES_|AirCmd|Shortcut|HoneySpace|SGM|VRI\[" |
        tail -n 1
}

# 32 or 64 for the process that loads effect plug-ins, empty when it cannot be identified.
rfx_factory_bits() {
    for pid in $(rfx_effect_loader_pids); do
        abi="$(rfx_elf_abi "/proc/$pid/exe" 2>/dev/null)"
        case "$abi" in
            64\ *) echo 64; return ;;
            32\ *) echo 32; return ;;
        esac
        case "$(readlink "/proc/$pid/exe" 2>/dev/null)" in
            */lib64/*) echo 64; return ;;
            */lib/*) echo 32; return ;;
        esac
    done
}

rfx_classify_error() {
    case "$1" in
        *"not accessible for the namespace"*) echo NS ;;
        *"wrong ELF class"*|*"is 32-bit instead of 64-bit"*|*"unexpected e_machine"*) echo ABI ;;
        *"cannot locate symbol"*|*"undefined symbol"*) echo SYM ;;
        *"cannot open shared object file"*) echo DEP ;;
        *"Permission denied"*) echo SELINUX ;;
        *"FILE_NOT_FOUND"*) echo NOFILE ;;
        *"Could not find library"*|*"can't find"*) echo MISSING ;;
        *"dlopen failed: library"*"not found"*) echo MISSING ;;
        *"could not get library"*) echo MISSING ;;
        *" not exist"*) echo MISSING ;;
        *"Failed to parse"*|*"partially invalid"*) echo PARSE ;;
        "") echo NONE ;;
        *) echo OTHER ;;
    esac
}

# Full pass; writes $RFX_STATE/diagnosis as "<code>|<short>|<detail>|<fix>".
rfx_diagnose() {
    mode="$(rfx_factory_mode)"
    aidl_lib="$(rfx_aidl_lib)"

    # E02: the module has no build for this CPU.
    if [ -z "$(rfx_lib_targets)" ]; then
        rfx_diag E02 "No engine build exists for this CPU." \
            "ABIs: $(getprop ro.product.cpu.abilist)" \
            "Send this device's ABI list; the module needs a build for it."
        return
    fi

    # E07: the factory is AIDL and this package has no AIDL engine.
    if [ "$mode" = aidl ] && [ -z "$aidl_lib" ]; then
        rfx_diag E07 "This ROM uses the AIDL effect HAL, but this build has no AIDL engine for API $(rfx_api)." \
            "AIDL builds packaged: $(ls "$MODDIR"/engine/*/libresonancefx_aidl*.so 2>/dev/null | sed 's|.*/||' | tr '\n' ' ')" \
            "Update the module to 2.6.0 or newer."
        return
    fi

    # E15: no idea which factory generation this ROM has.
    case "$mode" in
        aidl) cfg_name=audio_effects_config.xml ;;
        legacy) cfg_name=audio_effects.xml ;;
        *)
            rfx_diag E15 "Could not detect which effect HAL this ROM uses." \
                "The factory mode is unknown; audio may still be starting." \
                "Wait a few seconds and run the Action again; if it persists, reboot."
            return
            ;;
    esac
    # E07 (32-bit): the module ships the AIDL front-end for the primary 64-bit ABI only.
    if [ "$mode" = aidl ] && [ "$(rfx_factory_bits)" = 32 ] &&
        ! ls "$MODDIR"/engine/armeabi-v7a/libresonancefx_aidl*.so >/dev/null 2>&1; then
        rfx_diag E07 "This ROM's AIDL effect HAL is 32-bit, but the AIDL engine is built for 64-bit only." \
            "Factory bitness: 32; packaged AIDL builds: $(ls "$MODDIR"/engine/*/libresonancefx_aidl*.so 2>/dev/null | sed 's|.*/||' | tr '\n' ' ')" \
            "This device needs a 32-bit AIDL build; report it so one can be added."
        return
    fi

    eff="$(rfx_first_config "$cfg_name")"
    # Pre-Android 8 (and ROMs with no XML) read the legacy .conf. Only when no XML exists: once
    # an XML parses, libeffects never falls back to the .conf.
    if [ "$mode" = legacy ] && [ -z "$eff" ]; then
        eff="$(rfx_first_conf)"
        cfg_name=audio_effects.conf
    fi

    # E16: this shell cannot see the audio processes, so nothing below can be verified.
    if [ -z "$(rfx_effect_pids)" ] && [ "$(getprop init.svc.audioserver 2>/dev/null)" = running ]; then
        rfx_diag E16 "This session cannot inspect the audio HAL (private PID namespace)." \
            "audioserver is running, but no audio process is visible from this shell; mounts and configs cannot be checked." \
            "Run the Action from the root manager's own screen, or reboot; the boot stage checks the same facts from init."
        return
    fi

    # E03: the engine must be visible to the process that loads effects.
    if ! rfx_lib_visible; then
        expected="$(rfx_expected_lib_path)"
        staged="$MODDIR/system/$(rfx_primary_libdir)/soundfx/$(rfx_active_lib)"
        candidates="$(rfx_active_lib_paths | tr '\n' ' ')"
        if [ -e "$expected" ]; then
            rfx_diag E03 "The module is installed, but the audio HAL cannot see its files." \
                "$expected exists here but not in the effect process (mount namespace not shared). Candidates: $candidates" \
                "Reboot; if it persists, set the root manager to the global mount namespace."
        elif [ -e "$staged" ]; then
            rfx_diag E03 "The engine is staged but not exposed to the audio HAL." \
                "Module copy: $staged; expected: $expected. The root manager did not mount the module's /system tree; the APEX bind fallback could not place the library. Candidates: $candidates" \
                "Reboot; if it persists, send the logs. The library must land in the effect HAL's APEX soundfx directory or /system/lib64/soundfx."
        else
            rfx_diag E03 "The module did not stage the engine library." \
                "Expected $expected; module directory: $MODDIR" \
                "Reinstall the module and reboot."
        fi
        return
    fi

    # E05: configs were found but none survived patching (AIDL missing is E07, above).
    if [ ! -s "$RFX_STATE/configs" ]; then
        if [ -s "$RFX_STATE/config_sources" ]; then
            rfx_diag E05 "The audio effects config was found but could not be patched." \
                "Sources: $(cut -d'|' -f1 "$RFX_STATE/config_sources" | tr '\n' ' ')" \
                "Send the ROM's audio_effects.xml and these logs; its shape is unsupported."
        else
            rfx_diag E04 "No audio effects config was found on this ROM." \
                "Searched /odm/etc, /vendor/etc/audio/sku_<sku>, /vendor/etc and /system/etc." \
                "Send the ROM name and these logs; the module does not know this ROM's config path."
        fi
        return
    fi

    # E04/E06: which configs carry us? A vendor factory can read a file outside the AOSP order
    # (Xiaomi /my_product, some MTK builds), so E06 is only certain when none of the configs the
    # module patched contains the entry. Prefer the AOSP-effective file when it carries us;
    # otherwise use the first vendor config that does.
    patched="$(cut -d'|' -f1 "$RFX_STATE/configs" 2>/dev/null | tr '\n' ' ')"
    carriers=""
    for c in "$eff" $patched; do
        [ -f "$c" ] || continue
        grep -q "$RFX_UUID" "$c" 2>/dev/null || continue
        case " $carriers " in *" $c "*) ;; *) carriers="$carriers $c" ;; esac
    done
    if [ -z "$carriers" ]; then
        if [ -z "$eff" ]; then
            rfx_diag E04 "No $cfg_name was found where the effect HAL reads it." \
                "Patched: ${patched:-none}" \
                "Send the ROM name and these logs; the module does not know this ROM's config path."
        else
            rfx_diag E06 "No audio effects config on this ROM contains a ResonanceFX entry." \
                "Factory=$mode; AOSP-effective: $eff; patched: ${patched:-none}" \
                "Reinstall 2.6.0 or newer; if it persists, send $eff and these logs."
        fi
        return
    fi
    chosen=""
    if [ -n "$eff" ]; then
        case " $carriers " in *" $eff "*) chosen="$eff" ;; esac
    fi
    [ -n "$chosen" ] || chosen="$(printf '%s\n' $carriers | head -n 1)"

    # E08: the entry must match the factory generation.
    entry="$(rfx_config_entry "$chosen")"
    lib="$(printf '%s' "$entry" | cut -d'|' -f1)"
    path="$(printf '%s' "$entry" | cut -d'|' -f2)"
    typ="$(printf '%s' "$entry" | cut -d'|' -f3)"
    if [ "$mode" = aidl ]; then
        if [ "$lib" != "$RFX_LIB_AIDL_NAME" ] || [ -z "$typ" ]; then
            rfx_diag E08 "The AIDL config entry has the wrong shape." \
                "library=${lib:-missing} type=${typ:-missing} in $chosen" \
                "Reinstall the module; the entry must use the AIDL build and its type UUID."
            return
        fi
    elif [ "$lib" = "$RFX_LIB_AIDL_NAME" ]; then
        rfx_diag E08 "A legacy effect HAL was given an AIDL entry." \
            "library=$lib in $chosen" \
            "Reinstall the module; this entry cannot be loaded by the legacy ABI."
        return
    fi

    # E09: the library the entry names must resolve where the factory looks.
    if [ -z "$path" ]; then
        rfx_diag E09 "The config entry has no library path." "$entry in $chosen" "Reinstall the module."
        return
    fi
    resolved="$(rfx_resolve_library "$path")"
    if [ -z "$resolved" ]; then
        rfx_diag E09 "The effect HAL cannot find the engine library." \
            "$path is not in /odm, /vendor or /system soundfx directories (as seen by the audio HAL)." \
            "Reinstall the module; if it persists, send these logs."
        return
    fi

    # Registered? Then the load worked and old log lines are irrelevant.
    if rfx_engine_registered; then
        rfx_diag OK "Engine registered." "factory=$mode config=$chosen library=$resolved"
        return
    fi

    # E10: the file is there but cannot be loaded.
    elf="$(rfx_elf_abi "$resolved")"
    want="$(rfx_expected_elf_abi_for "$resolved")"
    if [ -n "$elf" ] && [ -n "$want" ] && [ "$elf" != "$want" ]; then
        rfx_diag E10 "The engine library has the wrong CPU architecture." \
            "$resolved is $elf; this HAL needs $want." \
            "Install the module build for this device."
        return
    fi
    err="$(rfx_factory_error_line)"
    kind="$(rfx_classify_error "$err")"
    case "$kind" in
        NS)
            rfx_diag E10 "The effect HAL's linker namespace rejects the engine library." "$err" \
                "Report this ROM: the library is in a partition this HAL cannot load from."
            return ;;
        ABI|SYM|DEP|SELINUX)
            rfx_diag E10 "The effect HAL cannot load the engine library ($kind)." "$err" \
                "Send this log; the build or its dependencies do not match this HAL."
            return ;;
        NOFILE)
            rfx_diag E06 "The config the effect HAL reads is missing." "$err" \
                "Reboot; if it persists, reinstall the module. The Action must not delete a live bind source."
            return ;;
        PARSE)
            rfx_diag E11 "The effect HAL rejected the patched config." "$err" \
                "Send the config and this log; the ROM's parser rejects an entry the module wrote."
            return ;;
        MISSING)
            rfx_diag E09 "The effect HAL could not find the engine library in its search path." "$err" \
                "Send this log."
            return ;;
        OTHER)
            rfx_diag E10 "The effect HAL reported an error while loading the engine." "$err" \
                "Send this log; it names the library or config the ROM rejected."
            return ;;
    esac

    # Nothing above matched: the config and library are correct but the factory has not loaded us.
    rfx_diag E14 "The effect HAL has not registered the engine yet." \
        "Config and library are in place (factory=$mode, $chosen, $resolved), but the HAL has not loaded it." \
        "Tap Action again or reboot; if it persists, send these logs."
}

# Writes the one-line module description: healthy keeps the tagline, a failure names the code and
# the reason so the root manager itself tells the user what is wrong.
rfx_set_verdict() {
    prop="$MODDIR/module.prop"
    [ -f "$prop" ] || return 0
    code="$1"
    short="$2"
    base="Free and open-source audio effects engine by Xeloria."
    case "$code" in
        OK) desc="✅ $short $base" ;;
        *) desc="❌ $code: $short Open the module's Action for details." ;;
    esac
    desc="$(printf '%s' "$desc" | tr '\n\r\t|' '    ')"
    awk -v d="$desc" '/^description=/{print "description=" d; next} {print}' "$prop" > "$prop.tmp" &&
        mv -f "$prop.tmp" "$prop"
}

# The app cannot read /data/adb, so publish the root-level verdict into its own files directory
# (root can write there; the app reads it without any permission). A timestamp and the format
# version make a stale copy harmless, so removing the module does not leave a lie on screen.
rfx_publish_diagnosis() {
    [ -f "$RFX_STATE/diagnosis" ] || return 0
    boot="$(cat /proc/sys/kernel/random/boot_id 2>/dev/null)"
    ver="$(sed -n 's/^versionCode=//p' "$MODDIR/module.prop" 2>/dev/null)"
    now="$(date +%s 2>/dev/null)"
    for d in "${RFX_APP_FILES:-/data/user/0/io.github.resonancefx/files}" /data/data/io.github.resonancefx/files; do
        [ -d "$d" ] || continue
        f="$d/module-diagnosis.txt"
        printf '# rfx-diagnosis f=1 v=%s t=%s boot=%s\n' "$ver" "$now" "$boot" > "$f" 2>/dev/null || continue
        cat "$RFX_STATE/diagnosis" >> "$f" 2>/dev/null || continue
        chmod 0644 "$f" 2>/dev/null
        uid="$(stat -c %u "$d" 2>/dev/null)"
        [ -n "$uid" ] && chown "$uid:$uid" "$f" 2>/dev/null
        return 0
    done
    # The app has not created its files directory yet; the next boot retries.
    return 0
}

