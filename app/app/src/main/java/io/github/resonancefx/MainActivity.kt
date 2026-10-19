// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Equalizer
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material.icons.outlined.LibraryMusic
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import io.github.resonancefx.service.EngineService
import io.github.resonancefx.ui.EffectsScreen
import io.github.resonancefx.ui.PresetsScreen
import io.github.resonancefx.ui.ResonanceTheme
import io.github.resonancefx.ui.SettingsScreen
import io.github.resonancefx.ui.StatusScreen

class MainActivity : ComponentActivity() {
    private val notificationPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= 33) notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
        EngineService.sync(this)

        setContent {
            ResonanceTheme {
                var tab by rememberSaveable { mutableIntStateOf(0) }
                Scaffold(
                    bottomBar = {
                        NavigationBar {
                            tabs.forEachIndexed { i, (label, icon) ->
                                NavigationBarItem(
                                    selected = tab == i,
                                    onClick = { tab = i },
                                    icon = { Icon(icon, contentDescription = null) },
                                    label = { Text(label) },
                                )
                            }
                        }
                    },
                ) { padding ->
                    AnimatedContent(
                        targetState = tab,
                        transitionSpec = { fadeIn() togetherWith fadeOut() },
                        modifier = Modifier.padding(padding),
                        label = "tab",
                    ) { current ->
                        when (current) {
                            0 -> EffectsScreen()
                            1 -> PresetsScreen()
                            2 -> StatusScreen()
                            else -> SettingsScreen()
                        }
                    }
                }
            }
        }
    }

    private companion object {
        val tabs: List<Pair<String, ImageVector>> = listOf(
            "Effects" to Icons.Outlined.Equalizer,
            "Presets" to Icons.Outlined.LibraryMusic,
            "Status" to Icons.Outlined.Info,
            "Settings" to Icons.Outlined.Settings,
        )
    }
}
