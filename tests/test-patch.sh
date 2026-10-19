#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Runs the module's config patchers against real-world configs in tests/fixtures and checks
# the output: exactly one ResonanceFX library and effect (stale ones replaced), well-formed XML,
# and idempotent re-patching.
#
# Usage: tests/test-patch.sh [shell]   e.g. tests/test-patch.sh "busybox sh"
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

# Everything the module writes goes under the sandbox, never to a real absolute path.
export RFX_DATA="$SANDBOX/data" MODDIR="$ROOT/module" RFX_LOGFILE=/dev/null
SHELL_CMD="${1:-bash}"

fail=0
for src in "$ROOT"/tests/fixtures/*; do
    name="$(basename "$src")"
    out="$SANDBOX/$name"
    # shellcheck disable=SC2016
    $SHELL_CMD -c '. "$MODDIR/common.sh"; rfx_patch_any "$1" "$2" && rfx_patch_any "$2" "$2.again"' _ "$src" "$out"
    if [ ! -f "$out.again" ]; then
        echo "FAIL  $name: patch failed"; fail=1; continue
    fi
    uuids=$(grep -ci "4daa8eaa-0842-4a8d-82e9-6eee5827fbe4" "$out.again")
    libs=$(grep -ciE '(name="resonancefx"|^\s*resonancefx \{)' "$out.again")
    xmlok=1
    case "$name" in
        *.xml) python -I -c "import sys,xml.dom.minidom as m; m.parse(sys.argv[1])" "$out.again" 2>/dev/null || xmlok=0 ;;
    esac
    if [ "$uuids" = 1 ] && [ "$libs" = 1 ] && [ "$xmlok" = 1 ] && cmp -s "$out" "$out.again"; then
        echo "PASS  $name"
    else
        echo "FAIL  $name: uuids=$uuids libs=$libs xml=$xmlok idempotent=$(cmp -s "$out" "$out.again" && echo y || echo n)"
        fail=1
    fi
done

exit $fail
