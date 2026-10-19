#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# KernelSU / APatch: runs after module files are mounted but before audio services start.
# If the overlay didn't reach /vendor (e.g. KernelSU on some Samsung firmware), fall back to
# bind mounts here so the very first audioserver start already sees the effect.
MODDIR=${0%/*}
RFX_LOGFILE=/data/adb/resonancefx/logs/boot.log
. "$MODDIR/common.sh"

rfx_log "post-mount: verifying"
rfx_ensure_mounted
touch "$RFX_STATE/post_mount_ran"
