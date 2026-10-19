#!/system/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
MODDIR=${0%/*}
RFX_LOGFILE=/dev/null
. "$MODDIR/common.sh"

rfx_unmount_all
rm -rf /data/adb/resonancefx
