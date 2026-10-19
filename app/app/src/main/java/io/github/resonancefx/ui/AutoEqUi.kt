// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Headphones
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material3.AssistChip
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.resonancefx.App
import io.github.resonancefx.autoeq.AutoEqDatabase
import io.github.resonancefx.autoeq.AutoEqProfile
import io.github.resonancefx.autoeq.Response
import io.github.resonancefx.driver.Params
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlin.math.abs

@Composable
private fun rememberAutoEqDatabase(): AutoEqDatabase? {
    val context = LocalContext.current
    val db by produceState<AutoEqDatabase?>(null) {
        value = withContext(Dispatchers.IO) { runCatching { AutoEqDatabase.get(context) }.getOrNull() }
    }
    return db
}

/** Selected headphone, its correction curve, and the picker. */
@Composable
fun AutoEqRow(value: String, strength: Int, onChange: (String) -> Unit) {
    val db = rememberAutoEqDatabase()
    var picking by remember { mutableStateOf(false) }
    val profile = db?.find(value)
    val status = rememberDriverStatus(5000)
    // Older engines don't answer GET_FEATURES at all (null), which also means no parametric EQ.
    val unsupported = status?.registered == true && (status.features ?: 0) and Params.FEATURE_PARAMETRIC_EQ == 0

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (unsupported) {
            Text(
                "The installed engine predates AutoEQ. Flash the latest ResonanceFX module and reboot.",
                color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall,
            )
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Outlined.Headphones, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    profile?.name ?: if (value.isBlank()) "No headphone selected" else "Unknown profile",
                    style = MaterialTheme.typography.titleSmall, maxLines = 2, overflow = TextOverflow.Ellipsis,
                )
                profile?.let {
                    Text("${it.form} · measured by ${it.sourceLabel}", style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            if (value.isNotBlank()) {
                IconButton(onClick = { onChange("") }) { Icon(Icons.Outlined.Close, contentDescription = "Clear") }
            }
        }
        profile?.let { p ->
            val (preamp, filters) = p.scaled(strength / 100f)
            ResponseCurve(Response.curve(preamp, filters), Modifier.fillMaxWidth().height(120.dp))
            Text(
                "${filters.size} filters · preamp %.1f dB".format(preamp),
                style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        FilledTonalButton(onClick = { picking = true }, enabled = db != null, modifier = Modifier.fillMaxWidth()) {
            if (db == null) CircularProgressIndicator(Modifier.width(18.dp).height(18.dp), strokeWidth = 2.dp)
            else Text(if (profile == null) "Choose headphone" else "Change headphone")
        }
        db?.let {
            Text(
                "${it.modelCount} models · ${it.profiles.size} measurements · AutoEq project",
                style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }

    if (picking && db != null) {
        AutoEqPicker(db, value, onDismiss = { picking = false }) { onChange(it); picking = false }
    }
}

@Composable
private fun AutoEqPicker(db: AutoEqDatabase, selected: String, onDismiss: () -> Unit, onPick: (String) -> Unit) {
    val route by App.instance.repository.route.collectAsStateWithLifecycle()
    var query by remember { mutableStateOf("") }
    val suggestions = remember(route) {
        if (route.key.startsWith("bluetooth:") || route.key.startsWith("usb:")) db.suggestionsFor(route.label.removePrefix("USB · "))
        else emptyList()
    }
    val results = remember(query) { db.search(query) }

    Dialog(onDismissRequest = onDismiss, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surface) {
            Column {
                Row(Modifier.padding(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = onDismiss) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back") }
                    OutlinedTextField(
                        value = query,
                        onValueChange = { query = it },
                        placeholder = { Text("Search ${db.modelCount} headphones") },
                        leadingIcon = { Icon(Icons.Outlined.Search, null) },
                        singleLine = true,
                        modifier = Modifier.weight(1f),
                    )
                }
                LazyColumn(contentPadding = PaddingValues(bottom = 24.dp)) {
                    if (query.isBlank() && suggestions.isNotEmpty()) {
                        item { SectionLabel("Matches for ${route.label}") }
                        items(suggestions, key = { "s:" + it.id }) { ProfileItem(it, it.id == selected, onPick) }
                        item { HorizontalDivider(Modifier.padding(vertical = 8.dp)) }
                        item { SectionLabel("All headphones") }
                    }
                    if (results.isEmpty()) {
                        item { Text("No match. Try fewer words, e.g. just the model number.", Modifier.padding(24.dp)) }
                    }
                    items(results, key = { it.id }) { ProfileItem(it, it.id == selected, onPick) }
                }
            }
        }
    }
}

@Composable
private fun SectionLabel(text: String) {
    Text(text, style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(start = 20.dp, top = 12.dp, bottom = 4.dp))
}

@Composable
private fun ProfileItem(p: AutoEqProfile, selected: Boolean, onPick: (String) -> Unit) {
    ListItem(
        headlineContent = { Text(p.name, fontWeight = if (selected) FontWeight.SemiBold else null) },
        supportingContent = { Text("${p.form} · ${p.sourceLabel}") },
        trailingContent = { if (selected) AssistChip(onClick = {}, label = { Text("Active") }) },
        modifier = Modifier.clickable { onPick(p.id) },
    )
}

/** Correction curve: 20 Hz–20 kHz on a log axis, ±12 dB. */
@Composable
fun ResponseCurve(db: List<Float>, modifier: Modifier = Modifier) {
    val scheme = MaterialTheme.colorScheme
    Canvas(modifier.clip(RoundedCornerShape(16.dp))) {
        val range = 12f
        drawRect(scheme.surfaceContainerHighest)
        listOf(-6f, 0f, 6f).forEach { level ->
            val y = size.height * (0.5f - level / (2 * range))
            drawLine(scheme.outlineVariant, Offset(0f, y), Offset(size.width, y), strokeWidth = 1.dp.toPx(),
                pathEffect = if (level == 0f) null else PathEffect.dashPathEffect(floatArrayOf(8f, 8f)))
        }
        // 100 Hz, 1 kHz, 10 kHz markers.
        listOf(100f, 1000f, 10000f).forEach { hz ->
            val x = size.width * (kotlin.math.log10(hz / 20f) / 3f)
            drawLine(scheme.outlineVariant.copy(alpha = 0.5f), Offset(x, 0f), Offset(x, size.height), strokeWidth = 1.dp.toPx())
        }
        val points = db.mapIndexed { i, v ->
            Offset(size.width * i / (db.size - 1), size.height * (0.5f - v.coerceIn(-range, range) / (2 * range)))
        }
        val path = Path().apply {
            moveTo(points.first().x, points.first().y)
            points.drop(1).forEach { lineTo(it.x, it.y) }
        }
        val fill = Path().apply {
            addPath(path)
            lineTo(size.width, size.height / 2)
            lineTo(0f, size.height / 2)
            close()
        }
        val peak = db.maxOf { abs(it) }.coerceAtLeast(1f)
        drawPath(fill, Brush.verticalGradient(listOf(scheme.tertiary.copy(alpha = 0.35f), scheme.primary.copy(alpha = 0.05f)),
            startY = size.height * (0.5f - peak / (2 * range)), endY = size.height * (0.5f + peak / (2 * range))))
        drawPath(path, scheme.primary, style = Stroke(width = 2.5.dp.toPx(), cap = StrokeCap.Round))
    }
}
