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
say "Effect processes: $(rfx_effect_pids | tr ' ' ' ')"
if [ "$(cat /proc/1/comm 2>/dev/null)" != init ]; then
    say "Note: this session is outside init's PID namespace; mounts made here do not reach the audio HAL."
fi
say ""
say "Engine libraries:"
for f in $(rfx_active_lib_paths); do
    if rfx_factory_sees "$f"; then say "  OK      $f"
    elif [ -e "$f" ]; then say "  HIDDEN  $f (exists, not visible to the audio HAL)"
    elif [ -e "$MODDIR$f" ]; then say "  STAGED  $f (module copy is not mounted)"
    else say "  MISSING $f"; fi
done
say ""
say "Effect configurations:"
if [ -s "$RFX_STATE/configs" ]; then
    while IFS='|' read -r cfg staged; do
        if rfx_factory_matches "$cfg" "$staged"; then say "  OK      $cfg"
        elif cmp -s "$staged" "$cfg" 2>/dev/null; then say "  HIDDEN  $cfg (applied, not visible to the audio HAL)"
        else say "  MISSING $cfg"; fi
    done < "$RFX_STATE/configs"
else
    say "  (none - no audio_effects config could be patched)"
fi
say ""
say "Effect factory: $(rfx_factory_mode)"
say "Config the framework reads:"
for name in audio_effects.xml audio_effects_config.xml; do
    first="$(rfx_first_config "$name")"
    [ -n "$first" ] && say "  $first"
done
true
say "Mounts:"
mounted="$(mount 2>/dev/null | grep -E 'soundfx|audio_effects' | sed 's/^/  /')"
if [ -n "$mounted" ]; then echo "$mounted" | while read -r l; do say "$l"; done
else say "  (none)"; fi
say ""
say "Repairing: re-patching configs and re-applying mounts..."
rfx_record_factory_mode
if rfx_detect_partitions; then
    say "  engine placement: legacy=$(rfx_legacy_partition) aidl=$(rfx_aidl_partition)"
    rfx_stage_libraries
fi
rfx_generate_configs
rfx_ensure_mounted
say "Restarting audio services..."
rfx_restart_audio

# The HAL needs time to restart and reload the patched configs (Samsung's
# vendor HALs regularly take longer than a few seconds), so poll for
# registration instead of checking once and reporting a false failure.
registered=0
tries=0
while [ "$tries" -lt 15 ]; do
    if rfx_engine_registered; then registered=1; break; fi
    sleep 2
    tries=$((tries + 1))
done

if [ "$registered" = "1" ]; then say "Audio HAL: engine registered"; else say "Audio HAL: engine NOT registered yet"; fi
rfx_diagnose
diag="$(rfx_diag_get 1)"
rfx_set_verdict "$diag" "$(rfx_diag_get 2)"
rfx_publish_diagnosis
if [ "$diag" = OK ]; then
    say "Result: ✅ $diag - $(rfx_diag_get 2)"
else
    say "Result: ❌ $diag - $(rfx_diag_get 2)"
fi
detail="$(rfx_diag_get 3)"
[ -n "$detail" ] && say "Detail: $detail"
fix="$(rfx_diag_get 4)"
[ -n "$fix" ] && say "Fix: $fix"
say "Codes are explained in DIAGNOSTICS.md."

# The most recent effect-factory error about the engine, if any. Framework noise (activity
# manager, freezer, notifications) is filtered out by rfx_factory_error_line.
err="$(rfx_factory_error_line)"
[ -n "$err" ] && say "  factory log: $err"
