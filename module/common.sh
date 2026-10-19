# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# ResonanceFX shared module logic. Sourced by customize.sh, post-fs-data.sh, post-mount.sh,
# service.sh, action.sh and uninstall.sh. POSIX sh only (runs under Magisk/KernelSU/APatch
# busybox ash and under toybox mksh).
#
# Callers must set MODDIR (the installed module directory, or $MODPATH during install).

RFX_ID=resonancefx
RFX_UUID=4daa8eaa-0842-4a8d-82e9-6eee5827fbe4
RFX_LIB=libresonancefx.so
RFX_LIB_NAME=resonancefx
RFX_EFFECT_NAME=resonancefx_standard
# Overridable only so the host-side tests can sandbox everything under a temp directory.
RFX_DATA="${RFX_DATA:-/data/adb/resonancefx}"
RFX_STAGE=$RFX_DATA/stage
RFX_LOGDIR=$RFX_DATA/logs
RFX_STATE=$RFX_DATA/state

# Our own library/effect names. Entries with these names (or our UUID) are removed before ours
# is added, so re-patching an already patched config never duplicates it.
RFX_OWN_NAMES="$RFX_LIB_NAME|$RFX_EFFECT_NAME"

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

# Partitions an effect library may be loaded from. The audio HAL (Android 8+) lives in the
# vendor linker namespace and can only dlopen from /vendor or /odm; audioserver on older
# releases loads from /system. We install to /vendor and /system on every device.
rfx_lib_partitions() {
    echo /vendor
    [ "$(rfx_api)" -lt 26 ] && echo /system
    true
}

# Android 8+ loads effects in the vendor audio HAL; /system copies of the configs are read only
# by framework processes that can't load vendor libraries anyway (Samsung's audioserver logs
# "library not found" for them), so they're left alone.
rfx_config_wanted() {
    case "$1" in
        /system/vendor/*) return 0 ;;
        /system/*) [ "$(rfx_api)" -lt 26 ] ;;
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

# rfx_patch_xml <src> <dst>: writes a copy of src with exactly one ResonanceFX library and
# effect entry. Returns 1 (and writes nothing) if src has no <libraries>/<effects> sections.
rfx_patch_xml() {
    src="$1"; dst="$2"
    grep -q '<libraries' "$src" 2>/dev/null || return 1
    grep -q '<effects' "$src" 2>/dev/null || return 1
    mkdir -p "$(dirname "$dst")"
    awk -v names="$RFX_OWN_NAMES" -v uuid="$RFX_UUID" \
        -v lib="        <library name=\"$RFX_LIB_NAME\" path=\"$RFX_LIB\"/>" \
        -v eff="        <effect name=\"$RFX_EFFECT_NAME\" library=\"$RFX_LIB_NAME\" uuid=\"$RFX_UUID\"/>" '
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
        *) rfx_patch_xml "$1" "$2" ;;
    esac
}

# ---- Module layout --------------------------------------------------------------------------

# Copies the right engine build into every soundfx directory the module overlays.
# Engine builds live in $MODDIR/engine/<abi>/libresonancefx.so.
rfx_stage_libraries() {
    # Copies from older versions that still targeted /system on Android 8+.
    [ "$(rfx_api)" -ge 26 ] && rm -rf "$MODDIR/system/lib/soundfx" "$MODDIR/system/lib64/soundfx"
    rfx_lib_targets | while read -r libdir abi; do
        srcLib="$MODDIR/engine/$abi/$RFX_LIB"
        if [ ! -f "$srcLib" ]; then
            rfx_log "missing engine build for $abi"
            continue
        fi
        for part in $(rfx_lib_partitions); do
            case "$part" in
                /vendor) dir="$MODDIR/system/vendor/$libdir/soundfx" ;;
                *) dir="$MODDIR/system/$libdir/soundfx" ;;
            esac
            mkdir -p "$dir"
            cp -f "$srcLib" "$dir/$RFX_LIB"
            chmod 0755 "$dir"
            chmod 0644 "$dir/$RFX_LIB"
            chcon "$(rfx_lib_context "$part/")" "$dir/$RFX_LIB" 2>/dev/null
            rfx_log "staged $abi -> $part/$libdir/soundfx/$RFX_LIB"
        done
    done
}

# Regenerates every patched config from the *current* stock files. Run at install time and
# again early on every boot, so OTA updates and vendor config changes never leave a stale copy.
# Records each target in $RFX_STATE/configs as "<real path>|<patched copy>".
rfx_generate_configs() {
    : > "$RFX_STATE/configs"
    # Drop previous generated copies so configs removed by an OTA don't linger in the overlay.
    # KernelSU moves system/vendor (and odm, product, system_ext) to the module root and leaves a
    # symlink behind, which find doesn't follow, so walk every partition directory explicitly.
    for d in system vendor odm product system_ext; do
        [ -d "$MODDIR/$d" ] && find "$MODDIR/$d/" -type f -name 'audio_effects*' -exec rm -f {} \; 2>/dev/null
    done
    rm -rf "$RFX_STAGE/cfg"
    mkdir -p "$RFX_STAGE/cfg"

    rfx_find_configs | while read -r cfg; do
        rfx_config_wanted "$cfg" || { rfx_log "left alone $cfg (framework copy)"; continue; }
        overlay="$(rfx_overlay_path "$cfg")"
        # Always keep a staged copy for the bind-mount fallback.
        staged="$RFX_STAGE/cfg$(echo "$cfg" | sed 's|/|__|g')"
        if rfx_patch_any "$cfg" "$staged"; then
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
    done
}

# ---- Verification & fallback ----------------------------------------------------------------

rfx_lib_visible() {
    # True if every soundfx dir the HAL may use contains our library.
    ok=0
    for pair in $(rfx_lib_targets | tr ' ' ':'); do
        libdir="${pair%%:*}"
        [ -f "/vendor/$libdir/soundfx/$RFX_LIB" ] || ok=1
    done
    return $ok
}

# Bind-mounts a soundfx directory that also contains our library over the real one. Used when
# the root implementation didn't overlay /vendor (e.g. KernelSU on some Samsung firmware).
rfx_bind_libraries() {
    rfx_lib_targets | while read -r libdir abi; do
        for part in $(rfx_lib_partitions); do
            target="$part/$libdir/soundfx"
            [ -f "$target/$RFX_LIB" ] && continue
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
            cp -f "$MODDIR/engine/$abi/$RFX_LIB" "$stage/$RFX_LIB"
            chmod 0755 "$stage"
            chmod 0644 "$stage/$RFX_LIB"
            chcon "$(rfx_context_of "$target")" "$stage" 2>/dev/null
            chcon "$(rfx_lib_context "$target/")" "$stage/$RFX_LIB" 2>/dev/null
            if mount --bind "$stage" "$target"; then
                rfx_log "bind-mounted library dir $target"
                echo bind > "$RFX_STATE/lib_mode"
            else
                rfx_log "FAILED to bind-mount $target"
            fi
        done
    done
}

# Bind-mounts each patched config whose real path doesn't contain our UUID yet.
rfx_bind_configs() {
    [ -f "$RFX_STATE/configs" ] || return 0
    while IFS='|' read -r cfg staged; do
        [ -f "$staged" ] || continue
        cmp -s "$staged" "$cfg" && continue
        if mount --bind "$staged" "$cfg"; then
            rfx_log "bind-mounted $cfg"
            echo bind > "$RFX_STATE/cfg_mode"
        else
            rfx_log "FAILED to bind-mount $cfg"
        fi
    done < "$RFX_STATE/configs"
}

rfx_configs_missing() {
    n=0
    [ -f "$RFX_STATE/configs" ] || { echo 0; return; }
    while IFS='|' read -r cfg staged; do
        cmp -s "$staged" "$cfg" 2>/dev/null || n=$((n + 1))
    done < "$RFX_STATE/configs"
    echo $n
}

# A bind-mounted soundfx dir can't be unmounted while the audio HAL has our library mapped
# (EBUSY), so an updated engine is swapped into the bind source instead: the mounted view
# changes immediately, the old inode stays valid for the running HAL until it restarts.
rfx_refresh_bound_libraries() {
    rfx_lib_targets | while read -r libdir abi; do
        for part in $(rfx_lib_partitions); do
            target="$part/$libdir/soundfx"
            stage="$RFX_STAGE/$(echo "$target" | sed 's|/|_|g')"
            new="$MODDIR/engine/$abi/$RFX_LIB"
            [ -f "$stage/$RFX_LIB" ] && [ -f "$new" ] || continue
            cmp -s "$new" "$target/$RFX_LIB" && continue
            cp -f "$new" "$stage/.$RFX_LIB.new" &&
                chmod 0644 "$stage/.$RFX_LIB.new" &&
                chcon "$(rfx_lib_context "$target/")" "$stage/.$RFX_LIB.new" 2>/dev/null
            mv -f "$stage/.$RFX_LIB.new" "$stage/$RFX_LIB" && rfx_log "refreshed engine in $target"
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
    if [ -f "$RFX_STATE/configs" ]; then
        while IFS='|' read -r cfg staged; do
            umount "$cfg" 2>/dev/null
        done < "$RFX_STATE/configs"
    fi
    for pair in $(rfx_lib_targets | tr ' ' ':'); do
        libdir="${pair%%:*}"
        for part in $(rfx_lib_partitions); do umount "$part/$libdir/soundfx" 2>/dev/null; done
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

