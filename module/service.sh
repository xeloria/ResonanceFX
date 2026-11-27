#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Late boot: final verification, fallback for managers without post-mount, diagnostics.
MODDIR=${0%/*}
RFX_LOGFILE=/data/adb/resonancefx/logs/boot.log
. "$MODDIR/common.sh"

if rfx_in_recovery; then
    # Never touch mounts or descriptions from a recovery session: the target system is not
    # running and any bind would die with the session.
    rfx_log "service: recovery session, skipping"
    exit 0
fi

# All mounts are applied after sys.boot_completed below. Doing them before boot can trip the
# boot watchdog on Samsungs and is unnecessary: the HAL is restarted afterwards anyway (#17).
late_fix=0
rm -f "$RFX_STATE/post_mount_ran"

# Wait for boot so the logcat below covers the effect factory's first load.
until [ "$(getprop sys.boot_completed)" = "1" ]; do sleep 2; done

# Apply the engine bind once after boot. A private HAL mount namespace can hide the module's
# overlay from the factory even when the shell sees it (Samsung/KernelSU Next, #17), so do not
# trust rfx_lib_visible alone; the bind is idempotent and the audio services are restarted below.
# post-mount already applied the binds before audio started, so only fall back when the loader
# still cannot see the engine or a config (overlay unavailable, late mount, private namespace).
if ! rfx_lib_visible || [ "$(rfx_configs_missing)" -gt 0 ]; then
    rfx_log "service: library or configs still not visible after boot, re-applying"
    rfx_ensure_mounted
    late_fix=1
fi

# The effect factory's ABI only becomes visible once the audio HAL has its libraries mapped.
# If this boot assumed the other one (first boot after installing, an OTA, a HAL switch),
# re-patch with the right engine build/entry shape and restart once.
RFX_MODE_LIVE=
used="$(cat "$RFX_STATE/factory_used" 2>/dev/null)"
live="$(rfx_factory_mode_live)"
if [ "$live" != unknown ] && [ "$live" != "$used" ]; then
    rfx_log "service: effect factory is $live (configured for ${used:-unknown}), re-patching"
    echo "$live" > "$RFX_STATE/factory"
    rfx_generate_configs
    rfx_ensure_mounted
    late_fix=1
fi
# The effect factory may be a vendor process or live in audioserver, and each engine build has
# to be in a partition that process can dlopen. Re-check now that the HAL is up and its stock
# plug-ins are mapped: that is the evidence for where ours belongs (#17, #20).
if rfx_detect_partitions; then
    rfx_log "service: factory placement is legacy=$(rfx_legacy_partition) aidl=$(rfx_aidl_partition), re-staging"
    rfx_stage_libraries
    rfx_ensure_mounted
    late_fix=1
fi
[ "$late_fix" = "1" ] && rfx_restart_audio && sleep 5
sleep 5

missing="$(rfx_configs_missing)"
total="$(wc -l < "$RFX_STATE/configs" 2>/dev/null | tr -d ' ')"
mode="overlay"
# Two explicit conditions: the shell parses "[ a ] || [ b ] && c" as "(a || b) && c", which
# would report "bind" when only part of the system ended up bind-mounted.
if [ -f "$RFX_STATE/lib_mode" ] || [ -f "$RFX_STATE/cfg_mode" ]; then
    mode="bind"
fi

logcat -d -b all 2>/dev/null | \
    grep -iE "resonancefx|EffectsFactory|EffectsConfig|EffectConfig|audio_effects|soundfx|AHAL_Effect|LegacyEffect" | \
    tail -n 200 > "$RFX_LOGDIR/logcat.txt"
# "already exist" is expected: vendors that load several config files see our effect more than
# once and keep the first definition.
errors="$(grep -iE "resonancefx|$RFX_UUID" "$RFX_LOGDIR/logcat.txt" | grep -iE "fail|error|cannot|could not|not found|denied|skipping|not exist|dlopen|namespace" | grep -viE "already exist|audit|avc:|relabel|selinux|scontext=|tcontext=|permissive=|ActivityManager|ActivityTaskManager|Freecess|Notification|PackageManager|WindowManager|SurfaceFlinger|InputDispatcher|Launcher|skipping freeze|hiddenapi|reflection|initMbrain|Mbrain|DefaultDispatch|auditd|type=1400|AndroidRuntime|System\.err|RuntimeInit|app=io\.github\.resonancefx" | tail -n 1)"

# The factory can register a little after sys.boot_completed; poll once instead of writing a
# false failure into the module description.
tries=0
while [ "$tries" -lt 10 ]; do
    rfx_engine_registered && break
    sleep 3
    tries=$((tries + 1))
done

factory="$(rfx_factory_mode)"
rfx_diagnose
diag="$(rfx_diag_get 1)"
if [ "$diag" = OK ]; then
    rfx_set_verdict OK "Active: $total config(s), $mode mount, $factory factory."
else
    rfx_set_verdict "$diag" "$(rfx_diag_get 2)"
fi
rfx_publish_diagnosis
registered=no; rfx_engine_registered && registered=yes
rfx_log "service: factory=$factory pids=$(rfx_effect_pids | tr ' ' ' ') total=$total missing=$missing mode=$mode registered=$registered diagnosis=$diag errors=${errors:-none}"
