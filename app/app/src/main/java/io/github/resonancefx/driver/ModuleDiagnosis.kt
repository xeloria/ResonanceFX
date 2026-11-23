// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import android.os.SystemClock
import java.io.File

/**
 * The root-level verdict published by the module into the app's files directory
 * (`module-diagnosis.txt`). The app cannot read /data/adb, so this is the channel that lets the
 * Status screen show the module's exact code and reason instead of only the app-side subset.
 */
data class ModuleDiagnosis(
    val code: String,
    val short: String,
    val detail: String,
    val fix: String,
    val moduleVersion: Int?,
) {
    companion object {
        private const val HEADER = "# rfx-diagnosis "
        private const val STALE_MS = 120_000L

        fun read(
            filesDir: File,
            now: Long = System.currentTimeMillis(),
            uptime: Long = SystemClock.elapsedRealtime(),
            currentBoot: String? = bootId(),
        ): ModuleDiagnosis? {
            val file = File(filesDir, "module-diagnosis.txt")
            if (!file.isFile) return null
            val lines = runCatching { file.readLines() }.getOrNull() ?: return null
            val header = lines.firstOrNull() ?: return null
            if (!header.startsWith(HEADER)) return null
            val fields = header.removePrefix(HEADER).trim().split(' ')
                .mapNotNull { part ->
                    val at = part.indexOf('=')
                    if (at <= 0) null else part.substring(0, at) to part.substring(at + 1)
                }
                .toMap()
            if (fields["f"] != "1") return null
            // A copy from an earlier boot is not evidence about the running system. The boot id is
            // exact; fall back to the write time when /proc is not readable.
            if (!currentBoot.isNullOrEmpty()) {
                if (fields["boot"] != currentBoot) return null
            } else {
                val writtenAt = fields["t"]?.toLongOrNull()?.times(1000) ?: return null
                if (writtenAt < now - uptime - STALE_MS) return null
            }
            val body = lines.getOrNull(1) ?: return null
            val parts = body.split('|')
            if (parts.size < 2 || parts[0].isBlank()) return null
            return ModuleDiagnosis(
                code = parts[0],
                short = parts.getOrElse(1) { "" },
                detail = parts.getOrElse(2) { "" },
                fix = parts.getOrElse(3) { "" },
                moduleVersion = fields["v"]?.toIntOrNull(),
            )
        }

        private fun bootId(): String? =
            runCatching { File("/proc/sys/kernel/random/boot_id").readText().trim() }.getOrNull()
    }
}
