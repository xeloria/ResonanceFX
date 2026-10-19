// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material.icons.outlined.GraphicEq
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.resonancefx.App
import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.DriverStatus
import io.github.resonancefx.model.AutoEqControl
import io.github.resonancefx.model.BandsControl
import io.github.resonancefx.model.GraphicEqControl
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.model.Choice
import io.github.resonancefx.model.Control
import io.github.resonancefx.model.Equalizer
import io.github.resonancefx.model.FileControl
import io.github.resonancefx.model.Section
import io.github.resonancefx.model.Slider as SliderControl
import io.github.resonancefx.model.Toggle
import io.github.resonancefx.service.EngineService
import io.github.resonancefx.service.refreshMasterTile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext

@Composable
fun EffectsScreen() {
    val repo = App.instance.repository
    val settings by repo.settings.collectAsStateWithLifecycle()
    val route by repo.route.collectAsStateWithLifecycle()
    var showAdvanced by rememberSaveable { mutableStateOf(false) }
    val status = rememberDriverStatus()
    val context = LocalContext.current

    LazyColumn(
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            HeroCard(
                on = settings.bool(Catalog.KEY_MASTER),
                profile = repo.activeProfileName.takeIf { repo.perDeviceProfiles } ?: "All outputs",
                status = status,
                onToggle = {
                    repo.set(Catalog.KEY_MASTER, it)
                    EngineService.sync(context)
                    refreshMasterTile(context)
                },
            )
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(selected = showAdvanced, onClick = { showAdvanced = !showAdvanced }, label = { Text("Advanced modules") })
                FilterChip(selected = false, onClick = {}, enabled = false, label = { Text(route.label) })
            }
        }
        // Until the engine reports its features (not attached yet), show everything.
        val features = status?.features
        val visible = Catalog.sections.filter { s ->
            (showAdvanced || !s.advanced) && (s.feature == 0 || features == null || features and s.feature != 0)
        }
        items(visible, key = { it.id }) { section ->
            SectionCard(section, settings) { key, value -> repo.set(key, value) }
        }
        item { Spacer(Modifier.height(24.dp)) }
    }
}

/** Polls the driver while the screen is visible. */
@Composable
fun rememberDriverStatus(intervalMs: Long = 2000): DriverStatus? {
    var status by remember { mutableStateOf<DriverStatus?>(null) }
    LaunchedEffect(Unit) {
        while (true) {
            status = withContext(Dispatchers.IO) { runCatching { DriverStatus.query() }.getOrNull() }
            delay(intervalMs)
        }
    }
    return status
}

@Composable
private fun HeroCard(on: Boolean, profile: String, status: DriverStatus?, onToggle: (Boolean) -> Unit) {
    val scheme = MaterialTheme.colorScheme
    val transition = rememberInfiniteTransition(label = "hero")
    val phase by transition.animateFloat(
        initialValue = 0f, targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(6000, easing = LinearEasing), RepeatMode.Reverse), label = "phase",
    )
    val pulse by transition.animateFloat(
        initialValue = 0.35f, targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(900, easing = FastOutSlowInEasing), RepeatMode.Reverse), label = "pulse",
    )
    val glow by animateFloatAsState(if (on) 1f else 0f, tween(600), label = "glow")
    val start by animateColorAsState(if (on) scheme.primaryContainer else scheme.surfaceContainerHigh, label = "start")
    val end by animateColorAsState(if (on) scheme.secondaryContainer else scheme.surfaceContainer, label = "end")

    Card(
        shape = RoundedCornerShape(28.dp),
        colors = CardDefaults.cardColors(containerColor = Color.Transparent),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Box(
            Modifier
                .background(Brush.linearGradient(listOf(start, end), start = androidx.compose.ui.geometry.Offset(phase * 600f, 0f)))
                .padding(24.dp),
        ) {
            Column {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        Icons.Outlined.GraphicEq, contentDescription = null,
                        tint = scheme.primary.copy(alpha = 0.4f + 0.6f * glow),
                        modifier = Modifier.size(36.dp),
                    )
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text("ResonanceFX", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
                        Text("Profile · $profile", style = MaterialTheme.typography.bodyMedium, color = scheme.onSurfaceVariant)
                    }
                    Switch(checked = on, onCheckedChange = onToggle)
                }
                Spacer(Modifier.height(16.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    val (color, text) = when {
                        status == null -> scheme.outline to "Checking engine…"
                        !status.registered -> scheme.error to "Engine not loaded — install the module and reboot"
                        !on -> scheme.outline to "Effects off"
                        status.sessionError != null -> scheme.error to status.sessionError
                        status.processing == true -> scheme.secondary to "Processing · ${status.sampleRate ?: "?"} Hz · ${status.channels ?: 2} ch"
                        else -> scheme.primary to status.headline
                    }
                    Box(
                        Modifier
                            .size(10.dp)
                            .alpha(if (status?.processing == true && on) pulse else 1f)
                            .clip(CircleShape)
                            .background(color),
                    )
                    Spacer(Modifier.width(8.dp))
                    Text(text, style = MaterialTheme.typography.labelLarge)
                }
            }
        }
    }
}

@Composable
private fun SectionCard(section: Section, settings: Settings, onChange: (String, Any) -> Unit) {
    var expanded by rememberSaveable(section.id) { mutableStateOf(section.enableKey == null) }
    val enabled = section.enableKey?.let { settings.bool(it) } ?: true
    val rotation by animateFloatAsState(if (expanded) 180f else 0f, label = "chevron")
    val container by animateColorAsState(
        if (enabled && section.enableKey != null) MaterialTheme.colorScheme.surfaceContainerHigh
        else MaterialTheme.colorScheme.surfaceContainer,
        label = "container",
    )

    Card(shape = RoundedCornerShape(22.dp), colors = CardDefaults.cardColors(containerColor = container)) {
        Column {
            Row(
                Modifier
                    .fillMaxWidth()
                    .clickable(enabled = section.controls.isNotEmpty()) { expanded = !expanded }
                    .padding(horizontal = 20.dp, vertical = 16.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f)) {
                    Text(section.title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Medium)
                    Text(section.summary, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                if (section.controls.isNotEmpty()) {
                    Icon(Icons.Filled.ExpandMore, contentDescription = null, modifier = Modifier.rotate(rotation))
                    Spacer(Modifier.width(8.dp))
                }
                section.enableKey?.let { key ->
                    Switch(checked = enabled, onCheckedChange = { onChange(key, it) })
                }
            }
            AnimatedVisibility(
                visible = expanded && section.controls.isNotEmpty(),
                enter = expandVertically() + fadeIn(),
                exit = shrinkVertically() + fadeOut(),
            ) {
                Column(
                    Modifier
                        .padding(start = 20.dp, end = 20.dp, bottom = 16.dp)
                        .alpha(if (enabled) 1f else 0.5f),
                    verticalArrangement = Arrangement.spacedBy(4.dp),
                ) {
                    section.controls.forEach { ControlRow(it, settings, onChange) }
                }
            }
        }
    }
}

@Composable
private fun ControlRow(control: Control, settings: Settings, onChange: (String, Any) -> Unit) {
    when (control) {
        is Toggle -> Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Text(control.label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
            Switch(checked = settings.bool(control.key), onCheckedChange = { onChange(control.key, it) })
        }
        is SliderControl -> SliderRow(control, settings.int(control.key)) { onChange(control.key, it) }
        is Choice -> ChoiceRow(control, settings.int(control.key)) { onChange(control.key, it) }
        is Equalizer -> EqualizerControl(settings.string(control.key)) { onChange(control.key, it) }
        is FileControl -> FileRow(control, settings.string(control.key)) { onChange(control.key, it) }
        is GraphicEqControl -> GraphicEqRow(settings.int(control.layoutKey) + 1, settings.string(control.key)) {
            onChange(control.key, it)
        }
        is BandsControl -> BandsEditor(settings.string(control.key), settings.int("peq.preamp")) { onChange(control.key, it) }
        is AutoEqControl -> AutoEqRow(settings.string(control.key), settings.int(Catalog.KEY_AUTOEQ_STRENGTH)) { onChange(control.key, it) }
    }
}

@Composable
private fun SliderRow(control: SliderControl, value: Int, onChange: (Int) -> Unit) {
    // Local state keeps dragging smooth; the engine gets every step, storage only the final one.
    var local by remember(value) { mutableFloatStateOf(value.toFloat()) }
    Column {
        Row {
            Text(control.label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
            Text(control.format(local.toInt()), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
        }
        Slider(
            value = local,
            onValueChange = {
                local = it
                if (it.toInt() != value) onChange(it.toInt())
            },
            valueRange = control.range.first.toFloat()..control.range.last.toFloat(),
            steps = (control.range.last - control.range.first - 1).coerceIn(0, 200).takeIf { control.range.last - control.range.first <= 24 } ?: 0,
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ChoiceRow(control: Choice, value: Int, onChange: (Int) -> Unit) {
    var open by remember { mutableStateOf(false) }
    ExposedDropdownMenuBox(expanded = open, onExpandedChange = { open = it }) {
        OutlinedTextField(
            value = control.options.getOrElse(value) { "" },
            onValueChange = {},
            readOnly = true,
            label = { Text(control.label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(open) },
            modifier = Modifier
                .fillMaxWidth()
                .menuAnchor(MenuAnchorType.PrimaryNotEditable),
        )
        ExposedDropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            control.options.forEachIndexed { i, option ->
                DropdownMenuItem(text = { Text(option) }, onClick = { onChange(i); open = false })
            }
        }
    }
}
