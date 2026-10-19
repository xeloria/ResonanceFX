// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext

private val Dark = darkColorScheme(
    primary = Color(0xFFB9A8FF),
    onPrimary = Color(0xFF26135F),
    primaryContainer = Color(0xFF3D2A86),
    onPrimaryContainer = Color(0xFFE6DEFF),
    secondary = Color(0xFF6EE7F2),
    onSecondary = Color(0xFF00363B),
    secondaryContainer = Color(0xFF004F56),
    onSecondaryContainer = Color(0xFF98F3FB),
    tertiary = Color(0xFFFFB3C8),
    background = Color(0xFF0E0D13),
    surface = Color(0xFF0E0D13),
    surfaceContainer = Color(0xFF1A1820),
    surfaceContainerHigh = Color(0xFF24222B),
    surfaceContainerHighest = Color(0xFF2F2C37),
    onSurface = Color(0xFFE6E1EA),
    onSurfaceVariant = Color(0xFFC9C3D3),
    outline = Color(0xFF928D9C),
    outlineVariant = Color(0xFF48454F),
)

private val Light = lightColorScheme(
    primary = Color(0xFF5845B6),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFE6DEFF),
    onPrimaryContainer = Color(0xFF170065),
    secondary = Color(0xFF006A72),
    secondaryContainer = Color(0xFF98F3FB),
    tertiary = Color(0xFF8E4359),
)

@Composable
fun ResonanceTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    val context = LocalContext.current
    val scheme: ColorScheme = when {
        Build.VERSION.SDK_INT >= 31 && dark -> dynamicDarkColorScheme(context)
        Build.VERSION.SDK_INT >= 31 -> dynamicLightColorScheme(context)
        dark -> Dark
        else -> Light
    }
    MaterialTheme(colorScheme = scheme, content = content)
}
