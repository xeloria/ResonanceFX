#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# End-to-end check on a rooted emulator/device reachable through adb (userdebug "default" image):
#   1. runs the module's real discovery/patch/bind logic on-device (toybox sh + awk),
#   2. restarts audio and verifies the HAL loads the engine,
#   3. installs the app, turns effects on, plays the test tone and checks the engine processes it.
# Uses the bind-mount path (no root manager needed); nothing is written to /system or /vendor.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APK="${APK:-$ROOT/app/app/build/outputs/apk/debug/app-debug.apk}"
PKG=io.github.resonancefx
step() { printf '\n== %s\n' "$*"; }
# Git Bash on Windows rewrites "/data/..." arguments into Windows paths; keep adb's literal.
adb() { MSYS_NO_PATHCONV=1 command adb "$@"; }
host_path() { cygpath -m "$1" 2>/dev/null || echo "$1"; }

step "root"
adb root >/dev/null || true
# A TCP-connected device (e.g. an emulator in WSL) drops the connection when adbd restarts.
case "${ANDROID_SERIAL:-}" in *:*) sleep 3; adb connect "$ANDROID_SERIAL" >/dev/null ;; esac
adb wait-for-device
until [ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = 1 ]; do sleep 2; done
adb shell 'getprop ro.build.version.release; getprop ro.product.cpu.abilist; id -u'

step "module: stage, patch, mount"
bash "$ROOT/scripts/build-module.sh" >/dev/null
adb shell rm -rf /data/local/tmp/rfx
adb push "$(host_path "$ROOT/build/module")" /data/local/tmp/rfx >/dev/null
adb shell 'MODDIR=/data/local/tmp/rfx RFX_LOGFILE=/data/local/tmp/rfx-test.log sh -c "
    . \$MODDIR/common.sh
    echo targets: \$(rfx_lib_targets | tr \"\n\" \" \")
    rfx_stage_libraries
    rfx_generate_configs
    echo configs:; cut -d\"|\" -f1 \$RFX_STATE/configs
    rfx_ensure_mounted
    echo missing after mount: \$(rfx_configs_missing)
    rfx_lib_visible && echo library visible || echo LIBRARY MISSING
    adb_logcat_mark=1
    logcat -c
    rfx_restart_audio
"'

step "audio HAL reload"
sleep 8
adb shell 'logcat -d | grep -iE "resonancefx|EffectsFactory|EffectConfig|EffectsConfig" | grep -iE "resonancefx|fail|error" | head -20' || true

step "app"
adb install -r -g "$(host_path "$APK")" >/dev/null
adb shell am force-stop $PKG
PROFILE="$ROOT/build/test-profile.json"
# PROFILE_JSON overrides the settings under test (e.g. every module on at once).
if [ -n "${PROFILE_JSON:-}" ]; then
    printf '%s
' "$PROFILE_JSON" > "$PROFILE"
else
    cat > "$PROFILE" <<'JSON'
{"format":"resonancefx.settings.v1","values":{"master.enable":true,"bass.enable":true,"bass.gain":6,"vhe.enable":true,"eq.enable":true,"eq.bands":"6.0;4.0;2.0;0.0;0.0;0.0;1.0;2.0;3.0;4.0"}}
JSON
fi
adb push "$(host_path "$PROFILE")" /data/local/tmp/rfx-profile.json >/dev/null
adb shell "chmod 644 /data/local/tmp/rfx-profile.json; run-as $PKG sh -c 'mkdir -p files/profiles && cp /data/local/tmp/rfx-profile.json files/profiles/speaker.json'"
adb logcat -c
adb shell am start -n $PKG/.MainActivity >/dev/null
sleep 6
adb shell am start-foreground-service -n $PKG/.service.EngineService -a $PKG.TEST_TONE >/dev/null
sleep 5

step "results"
adb shell 'logcat -d | grep -E "SessionManager|EngineEffect: Using|HiddenApi|configuration checked|disabled, reason|AndroidRuntime" | tail -20'
adb shell 'logcat -d -s ResonanceFX.Status:I -v raw' 
adb shell 'dumpsys media.audio_flinger | grep -iE -B2 -A6 "ResonanceFX|4daa8eaa" | head -40' || true
