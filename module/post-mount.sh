#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# KernelSU / APatch: runs after module files are mounted but before audio services start.
# If the overlay didn't reach /vendor (e.g. KernelSU on some Samsung firmware), fall back to
# bind mounts here so the very first audioserver start already sees the effect.
MODDIR=${0%/*}
RFX_LOGFILE=/data/adb/resonancefx/logs/boot.log
. "$MODDIR/common.sh"

if rfx_in_recovery; then rfx_log "post-mount: recovery session, skipping"; exit 0; fi

# Bind before audio starts so the HAL's mount namespace inherits the mounts (a bind made from
# the action/service shell does not reach a HAL that init starts afterwards). The expensive
# fallback copy is disabled here: post-mount must stay fast, or the boot watchdog reboots the
# device (#17). If the overlay is unavailable, service.sh applies the copy after boot.
rfx_log "post-mount: verifying (overlay only)"
RFX_NO_COPY=1 rfx_ensure_mounted
touch "$RFX_STATE/post_mount_ran"
