// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.service

import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.drawable.Icon
import android.media.audiofx.AudioEffect
import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import androidx.annotation.RequiresApi
import io.github.resonancefx.App
import io.github.resonancefx.R
import io.github.resonancefx.data.AttachMode
import io.github.resonancefx.model.Catalog

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val repo = App.instance.repository
        if (repo.startOnBoot && repo.settings.value.bool(Catalog.KEY_MASTER)) EngineService.start(context)
    }
}

/** Players announce their sessions here (used in per-app mode). */
class SessionReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val repo = App.instance.repository
        if (repo.attachMode != AttachMode.PER_APP || !repo.settings.value.bool(Catalog.KEY_MASTER)) return
        val action = when (intent.action) {
            AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION -> EngineService.ACTION_SESSION_OPEN
            AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION -> EngineService.ACTION_SESSION_CLOSE
            else -> return
        }
        EngineService.start(context, action, intent)
    }
}

@RequiresApi(Build.VERSION_CODES.N)
class MasterTileService : TileService() {
    override fun onStartListening() = render()

    override fun onClick() {
        val repo = App.instance.repository
        val on = !repo.settings.value.bool(Catalog.KEY_MASTER)
        repo.set(Catalog.KEY_MASTER, on)
        EngineService.sync(this)
        render()
    }

    private fun render() {
        val tile = qsTile ?: return
        val on = App.instance.repository.settings.value.bool(Catalog.KEY_MASTER)
        tile.state = if (on) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        tile.label = getString(R.string.app_name)
        if (Build.VERSION.SDK_INT >= 29) tile.subtitle = App.instance.repository.activeProfileName
        tile.icon = Icon.createWithResource(this, R.drawable.ic_stat_engine)
        tile.updateTile()
    }
}

/** Asks the system to re-render the Quick Settings tile (no-op below Android 7). */
fun refreshMasterTile(context: Context) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
        TileService.requestListeningState(context, ComponentName(context, MasterTileService::class.java))
    }
}
