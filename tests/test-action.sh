#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Runs module/action.sh against a stub common.sh and checks that the Action prints the
# diagnosis code from the shared decision tree, including the delayed-registration case.
#
# Usage: tests/test-action.sh   (RFX_ACTION_SH overrides the script under test)
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ACTION_SH="${RFX_ACTION_SH:-$ROOT/module/action.sh}"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

fail=0
check() { # name expected unexpected
    if grep -qF "$2" "$SANDBOX/out.txt" && ! grep -qF "$3" "$SANDBOX/out.txt"; then
        echo "PASS  $1"
    else
        echo "FAIL  $1"; sed 's/^/      /' "$SANDBOX/out.txt"; fail=1
    fi
}

run_case() { # fail_calls missing lib_ok [has_configs]
    rm -rf "$SANDBOX/mod"; mkdir -p "$SANDBOX/mod" "$SANDBOX/state" "$SANDBOX/bin"
    if [ "${4:-1}" = "1" ]; then printf 'cfg|staged\n' > "$SANDBOX/state/configs"
    else : > "$SANDBOX/state/configs"; fi
    cat > "$SANDBOX/mod/common.sh" <<EOF
RFX_LIB=libresonancefx.so
RFX_LIB_AIDL=libresonancefx_aidl.so
RFX_STATE="$SANDBOX/state"
RFX_LOGDIR="$SANDBOX"
RFX_UUID=00000000-0000-0000-0000-000000000000
RFX_TEST_MISSING=$2
RFX_TEST_LIB_OK=$3
rfx_log() { :; }
rfx_api() { echo 35; }
rfx_root_impl() { echo kernelsu; }
rfx_factory_mode() { echo legacy; }
rfx_record_factory_mode() { :; }
rfx_primary_abi() { echo arm64-v8a; }
rfx_aidl_lib() { :; }
rfx_effect_pids() { echo "\$\$"; }
rfx_active_lib() { echo "\$RFX_LIB"; }
rfx_factory_sees() { [ -f "\$1" ]; }
rfx_factory_matches() { cmp -s "\$1" "\$2"; }
rfx_lib_targets() { echo "lib64 arm64-v8a"; }
rfx_lib_partitions() { echo /vendor; }
rfx_generate_configs() { :; }
rfx_first_config() { :; }
rfx_ensure_mounted() { :; }
rfx_restart_audio() { :; }
rfx_configs_missing() { echo "\$RFX_TEST_MISSING"; }
rfx_lib_visible() { [ "\$RFX_TEST_LIB_OK" = "1" ]; }
rfx_engine_registered() {
    n=\$(( \$(cat "$SANDBOX/calls" 2>/dev/null || echo 0) + 1 ))
    echo \$n > "$SANDBOX/calls"
    [ \$n -gt $1 ]
}
rfx_diagnose() {
    if rfx_engine_registered; then c=OK; s="Engine registered."
    elif [ "\$RFX_TEST_MISSING" != "0" ]; then c=E06; s="The config has no ResonanceFX entry."
    elif ! rfx_lib_visible; then c=E03; s="The audio HAL cannot see the library."
    elif [ -s "\$RFX_STATE/configs" ]; then c=E14; s="The effect HAL has not registered the engine yet."
    else c=E04; s="No audio effects config."
    fi
    printf '%s|%s||\n' "\$c" "\$s" > "\$RFX_STATE/diagnosis"
}
rfx_diag_get() { cut -d'|' -f"\$1" "\$RFX_STATE/diagnosis"; }
rfx_set_verdict() { :; }
rfx_publish_diagnosis() { :; }
EOF
    printf '#!/bin/sh\necho test\n' > "$SANDBOX/bin/getprop"
    chmod +x "$SANDBOX/bin/getprop"
    echo 0 > "$SANDBOX/calls"
    cp "$ACTION_SH" "$SANDBOX/mod/action.sh"
    (cd "$SANDBOX/mod" && PATH="$SANDBOX/bin:$PATH" bash ./action.sh) > "$SANDBOX/out.txt" 2>/dev/null
}

# Registers on the third poll: must succeed despite the delay.
run_case 2 0 1
check "delayed registration succeeds" "Result: ✅ OK" "NOT registered"

# Never registers, nothing missing: E14, not a config error.
run_case 99 0 1
check "unregistered HAL reported as E14" "Result: ❌ E14" "cannot see"

# Configs genuinely not visible to the audio HAL: E06 keeps the real reason.
run_case 99 2 1
check "configs not visible reported as E06" "Result: ❌ E06" "E14"

# Library exists in the shell but not in the effect process's view: E03, not E06.
run_case 99 0 0
check "hidden library reported as E03" "Result: ❌ E03" "E06"

# No config could be patched: E04, and never "library and configs are visible".
run_case 99 0 1 0
check "no configs reported as E04" "Result: ❌ E04" "visible to the audio HAL"

exit $fail
