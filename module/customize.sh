# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# ResonanceFX installer (Magisk 20.4+, KernelSU, APatch).
SKIPUNZIP=0
MODDIR="$MODPATH"
RFX_LOGFILE=/data/adb/resonancefx/logs/install.log
mkdir -p /data/adb/resonancefx/logs
: > "$RFX_LOGFILE"
. "$MODPATH/common.sh"

API="$(rfx_api)"
ROOT_IMPL="$(rfx_root_impl)"

rfx_print "*********************************"
rfx_print "  ResonanceFX $(grep_prop version "$MODPATH/module.prop")"
rfx_print "  open-source audio effects engine"
rfx_print "*********************************"
rfx_print "- Device: $(getprop ro.product.manufacturer) $(getprop ro.product.model) ($(getprop ro.product.device))"
rfx_print "- Android API $API, root: $ROOT_IMPL"

if [ "$API" -lt 21 ]; then
    abort "! Android 5.0 (API 21) or newer is required"
fi

TARGETS="$(rfx_lib_targets)"
if [ -z "$TARGETS" ]; then
    abort "! Unsupported CPU ABI: $(getprop ro.product.cpu.abilist)"
fi
echo "$TARGETS" | while read -r libdir abi; do rfx_print "- Engine: $abi -> $libdir/soundfx"; done

rfx_print "- Installing engine libraries"
# An update starts from a clean mount layer: drop binds left by the previous version and remove
# its stage/state so a stale entry can never make the diagnostics lie. Recovery installs have no
# running system to clean up after (and mounts there die with the session).
if rfx_in_recovery; then
    rfx_print "- Recovery session: skipping mount cleanup"
else
    rfx_print "- Clearing leftovers from the previous install"
    rfx_unmount_all
fi
rm -rf "$RFX_STAGE" "$RFX_STATE/configs" "$RFX_STATE/config_sources" "$RFX_STATE/factory_used"
# Where each build belongs is a property of the running factory, not of the device name: ask it
# before staging, so the first boot already has the library in a loadable partition (#17, #20).
rfx_detect_partitions
rfx_stage_libraries

rfx_print "- Detecting the effect factory"
rfx_record_factory_mode
FACTORY="$(rfx_factory_mode)"
rfx_print "  + $FACTORY"
rfx_print "  + engine placement: legacy=$(rfx_legacy_partition) aidl=$(rfx_aidl_partition)"
if [ "$FACTORY" = aidl ] && [ -z "$(rfx_aidl_lib)" ]; then
    rfx_print "! [E07] This ROM uses the AIDL effect HAL and this build has no AIDL engine for API $API."
    rfx_print "! The effect engine will not load on this device."
fi

rfx_print "- Patching audio effects configurations"
rfx_snapshot_configs
rfx_generate_configs
COUNT="$(wc -l < "$RFX_STATE/configs" | tr -d ' ')"
if [ "$COUNT" -eq 0 ]; then
    if [ -s "$RFX_STATE/config_sources" ]; then
        rfx_print "! [E05] The audio effects config was found but could not be patched."
    else
        rfx_print "! [E04] No audio effects config was found on this ROM."
    fi
    rfx_print "! The effect will not load on this ROM. See $RFX_LOGFILE"
else
    cut -d'|' -f1 "$RFX_STATE/configs" | while read -r f; do rfx_print "  + $f"; done
fi

# KernelSU/APatch: nothing under system/ is used unless it's reachable through the overlay,
# and on these managers the module's own SELinux labels are what the HAL sees.
set_perm_recursive "$MODPATH" 0 0 0755 0644
for d in "$MODPATH/system/vendor/lib/soundfx" "$MODPATH/system/vendor/lib64/soundfx"; do
    [ -d "$d" ] && set_perm_recursive "$d" 0 0 0755 0644 u:object_r:vendor_file:s0
done
for d in "$MODPATH/system/lib/soundfx" "$MODPATH/system/lib64/soundfx"; do
    [ -d "$d" ] && set_perm_recursive "$d" 0 0 0755 0644 u:object_r:system_lib_file:s0
done
[ -d "$MODPATH/system/vendor/etc" ] && \
    find "$MODPATH/system/vendor/etc" -type f -name 'audio_effects*' -exec chcon u:object_r:vendor_configs_file:s0 {} \;
# Patched configs staged outside vendor/etc (system_ext, product, odm overlays) get the same
# permission treatment now, not only on the first post-fs-data pass.
find "$MODPATH/system" -type f \( -path '*/system_ext/etc/*' -o -path '*/product/etc/*' -o -path '*/odm/etc/*' \) \
    -name 'audio_effects*' -exec chcon u:object_r:system_file:s0 {} \; 2>/dev/null
find "$MODPATH/system/vendor/odm/etc" -type f -name 'audio_effects*' -exec chcon u:object_r:vendor_configs_file:s0 {} \; 2>/dev/null
for f in "$MODPATH"/*.sh; do chmod 0755 "$f"; done

rfx_print "- Done. Reboot to activate."
rfx_print "  Diagnostics: $RFX_LOGDIR"
