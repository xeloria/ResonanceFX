#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Late boot: final verification, fallback for managers without post-mount, diagnostics.
MODDIR=${0%/*}
RFX_LOGFILE=/data/adb/resonancefx/logs/boot.log
. "$MODDIR/common.sh"

late_fix=0
if [ ! -f "$RFX_STATE/post_mount_ran" ]; then
    # Magisk (or an old KernelSU without post-mount). Magic mount almost always works, but if
    # it didn't, audio services already started without us: fix up and restart them.
    if ! rfx_lib_visible || [ "$(rfx_configs_missing)" -gt 0 ]; then
        rfx_ensure_mounted
        late_fix=1
    fi
fi
rm -f "$RFX_STATE/post_mount_ran"

# Wait for boot so the logcat below covers the effect factory's first load.
until [ "$(getprop sys.boot_completed)" = "1" ]; do sleep 2; done
[ "$late_fix" = "1" ] && rfx_restart_audio && sleep 5
sleep 5

missing="$(rfx_configs_missing)"
total="$(wc -l < "$RFX_STATE/configs" 2>/dev/null | tr -d ' ')"
mode="overlay"
[ -f "$RFX_STATE/lib_mode" ] || [ -f "$RFX_STATE/cfg_mode" ] && mode="bind"

logcat -d -b main -b system 2>/dev/null | \
    grep -iE "resonancefx|EffectsFactory|EffectsConfig|EffectConfig|audio_effects|soundfx" | \
    tail -n 200 > "$RFX_LOGDIR/logcat.txt"
# "already exist" is expected: vendors that load several config files see our effect more than
# once and keep the first definition.
errors="$(grep -iE "resonancefx|$RFX_UUID" "$RFX_LOGDIR/logcat.txt" | grep -iE "fail|error|cannot|could not|not found|denied" | grep -viE "already exist" | head -n 1)"

if ! rfx_lib_visible; then
    rfx_set_description "❌ Engine library not mounted (see $RFX_LOGDIR)."
elif [ -z "$total" ] || [ "$total" = "0" ]; then
    rfx_set_description "❌ No audio effects config found on this ROM."
elif ! rfx_engine_registered; then
    rfx_set_description "❌ Audio HAL did not register the engine (see $RFX_LOGDIR)."
elif [ "$missing" -gt 0 ]; then
    rfx_set_description "⚠️ $missing of $total configs not applied ($mode)."
elif [ -n "$errors" ]; then
    rfx_set_description "⚠️ Loaded with errors: ${errors##*: }"
else
    rfx_set_description "✅ Active: $total config(s), $mode mount, $(rfx_lib_targets | awk '{print $2}' | tr '\n' ' ')"
fi
registered=no; rfx_engine_registered && registered=yes
rfx_log "service: total=$total missing=$missing mode=$mode registered=$registered errors=${errors:-none}"
