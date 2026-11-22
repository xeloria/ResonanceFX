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
    echo "$RFX_EFFECT_PIDS"
}

# rfx_factory_sees <absolute path>: true when at least one effect process can reach the path.
rfx_factory_sees() {
    _pids="$(rfx_effect_pids)"
    if [ -z "$_pids" ]; then [ -e "$1" ]; return; fi
    for pid in $_pids; do
        [ -e "/proc/$pid/root$1" ] && return 0
    done
    return 1
}

# rfx_factory_matches <path> <staged>: true when an effect process sees exactly the staged bytes.
rfx_factory_matches() {
    _pids="$(rfx_effect_pids)"
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

# Partitions a legacy effect library may be loaded from. The audio HAL (Android 8+) lives in
# the vendor linker namespace and can dlopen from /vendor or /odm; audioserver on older
# releases loads from /system.
rfx_lib_partitions() {
    echo /vendor
    [ "$(rfx_api)" -lt 26 ] && echo /system
    true
}

# Partitions a given engine build may be installed to. The AIDL build goes to /system only:
# an APEX-hosted AIDL effect HAL permits /system/lib64 but not /vendor/lib64, and the AOSP
# factory searches /odm, /vendor and /system in that order, so a vendor copy would be found
# first and then rejected by the linker namespace.
rfx_lib_partitions_for() {
    case "$1" in
        libresonancefx_aidl*) echo /system ;;
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

# True when the framework would parse this path: it is the first existing file with that name.
rfx_is_effective_config() {
    first="$(rfx_first_config "${1##*/}")"
    [ -n "$first" ] || return 1
    [ "$(readlink -f "$first" 2>/dev/null || echo "$first")" = "$1" ]
}

# Android 8+ normally loads effects in the vendor audio HAL, so /system copies are left alone:
# framework processes that read them can't load vendor libraries anyway (Samsung's audioserver
# logs "library not found" for them). The exception is a ROM with no readable vendor config at
# all: the framework then falls back to /system/etc, which must carry the effect too.
rfx_config_wanted() {
    case "$1" in
        /system/vendor/*) return 0 ;;
        /system/*)
            [ "$(rfx_api)" -lt 26 ] || rfx_is_effective_config "$1"
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
    mv -f "$dst.tmp" "$dst"
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
    mv -f "$dst.tmp" "$dst"
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
        # Drop builds that do not belong here, including ones an older module version staged:
        # the AIDL build must not be visible in /vendor (the factory would pick it first and
        # the HAL's namespace would reject it), and the legacy build is not used from /system
        # on Android 8+.
        for d in "$MODDIR/system/vendor/$libdir/soundfx" "$MODDIR/system/$libdir/soundfx"; do
            [ -d "$d" ] || continue
            rm -f "$d"/libresonancefx_aidl*.so
            [ "$(rfx_api)" -ge 26 ] && rm -f "$d/$RFX_LIB"
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

        # The AIDL front-end goes to /system, where an APEX-hosted effect HAL can reach it.
        for src in "$MODDIR/engine/$abi"/libresonancefx_aidl*.so; do
            [ -f "$src" ] || continue
            name="${src##*/}"
            dir="$MODDIR/system/$libdir/soundfx"
            mkdir -p "$dir"
            chmod 0755 "$dir"
            cp -f "$src" "$dir/$name"
            chmod 0644 "$dir/$name"
            chcon "$(rfx_lib_context "/system/")" "$dir/$name" 2>/dev/null
            rfx_log "staged $abi -> /system/$libdir/soundfx/$name"
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
    rm -rf "$RFX_STAGE/cfg"
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

rfx_lib_visible() {
    # True when the process that loads effects (not just this shell) can reach the library.
    ok=0
    name="$(rfx_active_lib)"
    case "$name" in
        libresonancefx_aidl*) libdirs="$(rfx_primary_libdir)" ;;
        *) libdirs="$(rfx_lib_targets | awk '{ print $1 }')" ;;
    esac
    for libdir in $libdirs; do
        for part in $(rfx_lib_partitions_for "$name"); do
            rfx_factory_sees "$part/$libdir/soundfx/$name" || ok=1
        done
    done
    return $ok
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
        for part in $(rfx_lib_partitions_for "$active"); do
            target="$part/$libdir/soundfx"
            rfx_factory_sees "$target/$active" && continue
            [ -d "$target" ] || continue
            stage="$RFX_STAGE/$(echo "$target" | sed 's|/|_|g')"
            rm -rf "$stage"
            mkdir -p "$stage"
            # Copy every stock effect library and keep its exact SELinux label, otherwise the
            # HAL would lose access to the vendor's own effects.
            for f in "$target"/*; do
                [ -f "$f" ] || continue
                cp -f "$f" "$stage/"
                chmod 0644 "$stage/${f##*/}"
                chcon "$(rfx_context_of "$f")" "$stage/${f##*/}" 2>/dev/null
            done
            lib="$MODDIR/engine/$abi/$active"
            if [ -f "$lib" ]; then
                cp -f "$lib" "$stage/$active"
                chmod 0644 "$stage/$active"
                chcon "$(rfx_lib_context "$target/")" "$stage/$active" 2>/dev/null
            fi
            chmod 0755 "$stage"
            chcon "$(rfx_context_of "$target")" "$stage" 2>/dev/null
            # Drop every layer left by an earlier pass or an earlier module version, then bind.
            rfx_drain_mount "$target"
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
        for part in $(rfx_lib_partitions_for "$active"); do
            target="$part/$libdir/soundfx"
            stage="$RFX_STAGE/$(echo "$target" | sed 's|/|_|g')"
            lib="$MODDIR/engine/$abi/$active"
            [ -f "$lib" ] || continue
            [ -f "$stage/$active" ] || continue
            cmp -s "$lib" "$target/$active" && continue
            cp -f "$lib" "$stage/.$active.new" &&
                chmod 0644 "$stage/.$active.new" &&
                chcon "$(rfx_lib_context "$target/")" "$stage/.$active.new" 2>/dev/null
            mv -f "$stage/.$active.new" "$stage/$active" && rfx_log "refreshed engine in $target"
        done
    done
}

rfx_ensure_mounted() {
    rfx_refresh_bound_libraries
    if ! rfx_lib_visible; then
        rfx_log "library not visible after module mount, applying bind fallback"
        rfx_bind_libraries
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
    done
}

# Restarts audioserver and every audio HAL so a late mount takes effect without a reboot.
rfx_restart_audio() {
    rfx_log "restarting audio services"
    # Restart through init (never kill -9) so services flagged "critical" aren't counted as
    # crashing. Covers audioserver plus every HIDL/AIDL audio HAL, whatever the vendor named it.
    getprop | sed -n 's/^\[init\.svc\.\([^]]*\)\].*/\1/p' | while read -r svc; do
        case "$svc" in
            audioserver|*audio*hal*|*hal*audio*|*audio-service*|*audio_service*)
                setprop ctl.restart "$svc"
                rfx_log "restarted $svc"
                ;;
        esac
    done
    # The effect processes are new now; forget the cached pids.
    RFX_EFFECT_PIDS=
}

# The definitive check: did the audio HAL's effect factory actually register our UUID?
# (audio_flinger dumps every effect library and descriptor it knows.)
rfx_engine_registered() {
    dumpsys media.audio_flinger 2>/dev/null | grep -qi "$RFX_UUID"
}

rfx_set_description() {
    prop="$MODDIR/module.prop"
    [ -f "$prop" ] || return 0
    base="Free and open-source audio effects engine by Xeloria."
    sed -i "s|^description=.*|description=$1 $base|" "$prop"
}

