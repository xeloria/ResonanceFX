// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import io.github.resonancefx.driver.DriverStatus
import io.github.resonancefx.driver.DriverStatusHub
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Driver status for the Status/Effects screens. The foreground service publishes a fresh
 * snapshot from its own watchdog tick; the UI never interrogates the effect handle itself.
 * (The old per-screen poll loops fired bursts of getter commands against the effect every
 * couple of seconds, and on a suspended/offloaded sound output those periodic effect-chain
 * round trips interrupted the music every tick — while on the Presets/Settings screens, which
 * never touch the handle, playback ran uninterrupted.)
 *
 * Only when the service hasn't published recently (service not running, e.g. before its
 * first start) does a single one-shot probe run from here — bounded, never periodic.
 */
@Composable
fun rememberDriverStatus(): DriverStatus? {
    LaunchedEffect(Unit) {
        if (System.currentTimeMillis() - DriverStatusHub.publishedAt > 8000) {
            withContext(Dispatchers.IO) { runCatching { DriverStatusHub.refresh() } }
        }
    }
    return DriverStatusHub.status.collectAsState().value
}
