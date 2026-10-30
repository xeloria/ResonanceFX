#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Runs module/action.sh against a stub common.sh and checks the verdict lines:
# registration that only appears after a delay still counts as success, and a
# HAL that never registers is reported as "not registered yet" (with zero
# configs missing), never as missing configs.
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

run_case() { # fail_calls missing lib_ok
    rm -rf "$SANDBOX/mod"; mkdir -p "$SANDBOX/mod" "$SANDBOX/state" "$SANDBOX/bin"
    printf 'cfg|staged\n' > "$SANDBOX/state/configs"
    cat > "$SANDBOX/mod/common.sh" <<EOF
RFX_LIB=libresonancefx.so
RFX_STATE="$SANDBOX/state"
RFX_LOGDIR="$SANDBOX"
RFX_UUID=00000000-0000-0000-0000-000000000000
rfx_log() { :; }
rfx_api() { echo 35; }
rfx_root_impl() { echo kernelsu; }
rfx_lib_targets() { echo "lib64 arm64-v8a"; }
rfx_lib_partitions() { echo /vendor; }
rfx_generate_configs() { :; }
rfx_ensure_mounted() { :; }
rfx_restart_audio() { :; }
rfx_configs_missing() { echo $2; }
rfx_lib_visible() { [ "$3" = "1" ]; }
rfx_engine_registered() {
    n=\$(( \$(cat "$SANDBOX/calls" 2>/dev/null || echo 0) + 1 ))
    echo \$n > "$SANDBOX/calls"
    [ \$n -gt $1 ]
}
EOF
    printf '#!/bin/sh\necho test\n' > "$SANDBOX/bin/getprop"
    chmod +x "$SANDBOX/bin/getprop"
    echo 0 > "$SANDBOX/calls"
    cp "$ACTION_SH" "$SANDBOX/mod/action.sh"
    (cd "$SANDBOX/mod" && PATH="$SANDBOX/bin:$PATH" bash ./action.sh) > "$SANDBOX/out.txt" 2>/dev/null
}

# Registers on the third poll: must succeed despite the delay.
run_case 2 0 1
check "delayed registration succeeds" "Result: ✅" "NOT registered"

# Never registers, nothing missing: must say so, not blame missing configs.
run_case 99 0 1
check "unregistered HAL reported honestly" "Result: ⚠️" "still missing"

# Configs genuinely missing: the failure keeps its real reason.
run_case 99 2 1
check "missing configs reported" "Result: ❌ 2 config(s) still missing" "⚠️"

exit $fail
