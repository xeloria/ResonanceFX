#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Runs before the root manager mounts module files: regenerate patched configs from the
# current stock files so an OTA or vendor update never leaves a stale copy in the overlay.
MODDIR=${0%/*}
RFX_LOGFILE=/data/adb/resonancefx/logs/boot.log
mkdir -p /data/adb/resonancefx/logs
mv -f "$RFX_LOGFILE" "$RFX_LOGFILE.old" 2>/dev/null
. "$MODDIR/common.sh"

rfx_log "post-fs-data: root=$(rfx_root_impl) api=$(rfx_api) abis=$(getprop ro.product.cpu.abilist)"
rm -f "$RFX_STATE/lib_mode" "$RFX_STATE/cfg_mode"

rfx_stage_libraries
rfx_generate_configs
rfx_log "post-fs-data: $(wc -l < "$RFX_STATE/configs" | tr -d ' ') config(s) staged"

# Magisk never overlays /odm (and some ROMs keep effect configs there): bind those right away.
# Everything else is verified after the module mount (post-mount.sh / service.sh).
if [ -f "$RFX_STATE/configs" ]; then
    while IFS='|' read -r cfg staged; do
        case "$cfg" in
            /odm/*|/my_product/*|/mi_ext/*|/vendor/odm/*)
                mount --bind "$staged" "$cfg" && rfx_log "bind-mounted $cfg" && echo bind > "$RFX_STATE/cfg_mode"
                ;;
        esac
    done < "$RFX_STATE/configs"
fi
