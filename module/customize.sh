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
rfx_stage_libraries

rfx_print "- Patching audio effects configurations"
rfx_generate_configs
COUNT="$(wc -l < "$RFX_STATE/configs" | tr -d ' ')"
if [ "$COUNT" -eq 0 ]; then
    rfx_print "! No audio effects configuration found to patch."
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
for f in "$MODPATH"/*.sh; do chmod 0755 "$f"; done

rfx_print "- Done. Reboot to activate."
rfx_print "  Diagnostics: $RFX_LOGDIR"
