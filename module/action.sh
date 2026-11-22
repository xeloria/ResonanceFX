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
active="$(rfx_active_lib)"
rfx_lib_targets | while read -r libdir abi; do
    for part in $(rfx_lib_partitions); do
        f="$part/$libdir/soundfx/$active"
        if [ ! -d "$part/$libdir/soundfx" ]; then say "  n/a     $f (not used by this ROM)"
        elif [ ! -f "$f" ]; then say "  MISSING $f"
        elif rfx_factory_sees "$f"; then say "  OK      $f"
        else say "  HIDDEN  $f (not visible to the audio HAL)"; fi
    done
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

missing="$(rfx_configs_missing)"
factory="$(rfx_factory_mode)"
pids="$(rfx_effect_pids)"
aidl_missing=0
[ "$factory" = aidl ] && [ -z "$(rfx_aidl_lib)" ] && aidl_missing=1
if [ "$registered" = "1" ]; then say "Audio HAL: engine registered"; else say "Audio HAL: engine NOT registered yet"; fi
if [ "$aidl_missing" = "1" ]; then
    say "Result: ❌ this ROM uses the AIDL effect HAL; this build only ships the legacy engine."
    say "The AIDL engine build (libresonancefx_aidl.so) is required for this device."
elif [ -z "$pids" ]; then
    say "Result: ⚠️ could not inspect the audio HAL from this session (private PID namespace)."
    say "Reboot so the boot-stage scripts verify and apply the mounts from init."
elif [ ! -s "$RFX_STATE/configs" ]; then
    say "Result: ❌ no audio effects config is installed, so the factory cannot see the engine."
    say "Reboot once: the boot stage snapshots the ROM's configs before the module mount."
elif ! rfx_lib_visible; then
    say "Result: ❌ the audio HAL cannot see $active."
    say "The mount is not shared with the effect process (stale bind or a private mount namespace)."
    say "Reboot so the boot-stage scripts apply it in init's namespace. Full log: $RFX_LOGDIR"
elif [ "$missing" != "0" ]; then
    say "Result: ❌ $missing config(s) not visible to the audio HAL. Full log: $RFX_LOGDIR"
elif [ "$registered" != "1" ]; then
    say "Result: ⚠️ library and configs are visible to the audio HAL, but it has not registered the engine yet."
    say "It may still be restarting - wait a few seconds and check the app's Status screen, or tap Action again."
else
    say "Result: ✅ engine and $(wc -l < "$RFX_STATE/configs" | tr -d ' ') config(s) in place."
    say "Open the ResonanceFX app: the Status screen confirms the effect is registered."
fi

# Recent effect-factory errors mentioning us, if any.
logcat -d -b all 2>/dev/null | grep -iE "resonancefx|$RFX_UUID" | \
    grep -iE "fail|error|cannot|could not|not found|denied|skipping|not exist" | \
    grep -viE "audit|avc:|relabel|selinux|scontext=|tcontext=|permissive=" | \
    tail -n 5 | while read -r l; do say "  log: $l"; done
