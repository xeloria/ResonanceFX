// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.model.Measurements
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlin.math.log10

/** "Engine measurements": every effect's measured behaviour, as published with the release. */
@Composable
fun MeasurementsDialog(onDismiss: () -> Unit) {
    val context = LocalContext.current
    var data by remember { mutableStateOf<Measurements?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) {
        withContext(Dispatchers.IO) {
            runCatching { Measurements.parse(context.assets.open("measurements.json").bufferedReader().readText()) }
                .onSuccess { data = it }
                .onFailure { error = "Measurements unavailable: ${it.message}" }
        }
    }
    Dialog(onDismissRequest = onDismiss, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surface) {
            LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                item {
                    Row {
                        Text(
                            "Engine measurements", Modifier.weight(1f),
                            style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Medium,
                        )
                        TextButton(onClick = onDismiss) { Text("Close") }
                    }
                    val d = data
                    Text(
                        "Measured, not claimed: what every effect actually does to the sound, measured on the " +
                            "engine itself" + (d?.let { " (version ${it.engine})" } ?: "") + ". The same numbers are " +
                            "published with every release.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    d?.method?.takeIf { it.isNotBlank() }?.let {
                        Text(
                            it, Modifier.padding(top = 4.dp),
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                }
                val d = data ?: return@LazyColumn
                val titles = Catalog.sections.associate { it.id to it.title }
                items(d.effects, key = { it.name }) { effect ->
                    EffectCard(effect, titles[effect.section], d.freqs)
                }
            }
        }
    }
}

@Composable
private fun EffectCard(e: Measurements.Effect, sectionTitle: String?, freqs: List<Float>) {
    val scheme = MaterialTheme.colorScheme
    Card(shape = RoundedCornerShape(20.dp), colors = CardDefaults.cardColors(containerColor = scheme.surfaceContainer)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(e.name, style = MaterialTheme.typography.titleMedium)
            if (sectionTitle != null && !e.name.startsWith(sectionTitle)) {
                Text(sectionTitle, style = MaterialTheme.typography.labelMedium, color = scheme.primary)
            }
            Text(e.settings, style = MaterialTheme.typography.bodySmall, color = scheme.onSurfaceVariant)
            ResponsePlot(freqs, listOfNotNull(e.response to scheme.primary, e.crosstalk?.let { it to scheme.tertiary }),
                Modifier.fillMaxWidth().height(120.dp))
            if (e.crosstalk != null) {
                Text("Purple: the same channel. Second curve: what leaks into the other channel.",
                    style = MaterialTheme.typography.bodySmall, color = scheme.onSurfaceVariant)
            }
            val facts = buildList {
                e.noiseDb?.let { add("Loudness change (pink noise): ${signed(it)} dB") }
                if (e.thd100 != null && e.thd1k != null) {
                    add("Distortion at -12 dBFS: ${pct(e.thd100)} at 100 Hz, ${pct(e.thd1k)} at 1 kHz")
                }
                if (e.gainLoud != null && e.gainQuiet != null) {
                    add("Loud tone (-3 dBFS): ${signed(e.gainLoud)} dB · quiet tone (-40 dBFS): ${signed(e.gainQuiet)} dB")
                }
                e.widthDb?.let { add("Stereo width (side/mid): ${signed(it)} dB") }
                e.tailDb?.let { add("Tail 0.3-1.3 s after a click: ${"%.0f".format(it)} dB") }
            }
            facts.forEach { Text(it, style = MaterialTheme.typography.bodySmall) }
        }
    }
}

private fun signed(v: Float) = "%+.1f".format(v)
private fun pct(v: Float) = if (v < 0.01f) "<0.01 %" else "%.2f %%".format(v)

/** Gain curves (dB) on a 20 Hz-20 kHz log axis, +-18 dB. */
@Composable
private fun ResponsePlot(freqs: List<Float>, curves: List<Pair<List<Float?>, Color>>, modifier: Modifier) {
    val scheme = MaterialTheme.colorScheme
    Canvas(modifier.clip(RoundedCornerShape(12.dp))) {
        val range = 18f
        fun x(hz: Float) = size.width * (log10(hz / 20f) / 3f)
        fun y(db: Float) = size.height * (0.5f - db.coerceIn(-range, range) / (2 * range))
        drawRect(scheme.surfaceContainerHighest)
        listOf(-12f, -6f, 0f, 6f, 12f).forEach { level ->
            drawLine(
                scheme.outlineVariant, Offset(0f, y(level)), Offset(size.width, y(level)), strokeWidth = 1.dp.toPx(),
                pathEffect = if (level == 0f) null else PathEffect.dashPathEffect(floatArrayOf(8f, 8f)),
            )
        }
        listOf(100f, 1000f, 10000f).forEach { hz ->
            drawLine(scheme.outlineVariant.copy(alpha = 0.5f), Offset(x(hz), 0f), Offset(x(hz), size.height), strokeWidth = 1.dp.toPx())
        }
        curves.forEach { (db, color) ->
            val path = Path()
            var started = false
            db.forEachIndexed { i, v ->
                if (v == null || i >= freqs.size) return@forEachIndexed
                if (!started) path.moveTo(x(freqs[i]), y(v)) else path.lineTo(x(freqs[i]), y(v))
                started = true
            }
            drawPath(path, color, style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round))
        }
    }
}
