#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# "Action" button in Magisk / KernelSU / APatch: diagnose and repair without rebooting.
MODDIR=${0%/*}
RFX_LOGFILE=/data/adb/resonancefx/logs/action.log
: > "$RFX_LOGFILE"
. "$MODDIR/common.sh"

say() { echo "$*"; rfx_log "$*"; }

say "ResonanceFX diagnostics"
say "======================="
say "Device : $(getprop ro.product.manufacturer) $(getprop ro.product.model)"
say "Android: $(getprop ro.build.version.release) (API $(rfx_api))"
say "Root   : $(rfx_root_impl)"
say ""
say "Engine libraries:"
rfx_lib_targets | while read -r libdir abi; do
    for part in $(rfx_lib_partitions); do
        f="$part/$libdir/soundfx/$RFX_LIB"
        if [ -f "$f" ]; then say "  OK      $f"
        elif [ ! -d "$part/$libdir/soundfx" ]; then say "  n/a     $part/$libdir/soundfx (not used by this ROM)"
        else say "  MISSING $f"; fi
    done
done
say ""
say "Effect configurations:"
if [ -f "$RFX_STATE/configs" ]; then
    while IFS='|' read -r cfg staged; do
        if cmp -s "$staged" "$cfg"; then say "  OK      $cfg"; else say "  MISSING $cfg"; fi
    done < "$RFX_STATE/configs"
else
    say "  (none recorded yet - reboot once after installing)"
fi
say ""

say "Repairing: re-patching configs and re-applying mounts..."
rfx_generate_configs
rfx_ensure_mounted
say "Restarting audio services..."
rfx_restart_audio
sleep 4

missing="$(rfx_configs_missing)"
if rfx_engine_registered; then say "Audio HAL: engine registered"; else say "Audio HAL: engine NOT registered"; fi
if rfx_lib_visible && [ "$missing" = "0" ] && rfx_engine_registered; then
    say "Result: ✅ engine and $(wc -l < "$RFX_STATE/configs" | tr -d ' ') config(s) in place."
    say "Open the ResonanceFX app: the Status screen confirms the effect is registered."
else
    say "Result: ❌ $missing config(s) still missing. Full log: $RFX_LOGDIR"
fi

# Recent effect-factory errors mentioning us, if any.
logcat -d -b main -b system 2>/dev/null | grep -iE "resonancefx|$RFX_UUID" | \
    grep -iE "fail|error|cannot|could not|not found|denied" | tail -n 5 | while read -r l; do say "  log: $l"; done
