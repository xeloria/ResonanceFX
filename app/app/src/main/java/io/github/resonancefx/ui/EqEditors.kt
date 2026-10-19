// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import io.github.resonancefx.autoeq.PeqFilter
import io.github.resonancefx.autoeq.Response
import io.github.resonancefx.model.Composite
import kotlin.math.ln
import kotlin.math.exp
import kotlin.math.roundToInt

/** 15- or 31-band graphic EQ; [layout] is the engine layout (1 = 15, 2 = 31 bands). */
@Composable
fun GraphicEqRow(layout: Int, value: String, onChange: (String) -> Unit) {
    val bandsHz = Composite.graphicEqBands(layout)
    val gains = remember(value, layout) { Composite.parseGains(value, bandsHz.size) }
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        EqCurve(gains, bandsHz) { onChange(Composite.formatGains(it)) }
        TextButton(onClick = { onChange("") }, modifier = Modifier.align(Alignment.End)) { Text("Flatten") }
    }
}

/** Parametric EQ band list with the combined response curve on top. */
@Composable
fun BandsEditor(value: String, preampDb: Int, onChange: (String) -> Unit) {
    val bands = remember(value) { Composite.parseBands(value) }
    val response = remember(value, preampDb) {
        Response.curve(preampDb.toFloat(), bands.map { PeqFilter(it.type, it.frequency, it.gainDb, it.q) })
    }
    fun update(i: Int, band: Composite.Band) = onChange(Composite.formatBands(bands.toMutableList().also { it[i] = band }))

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        ResponseCurve(response, Modifier.fillMaxWidth().height(140.dp))
        bands.forEachIndexed { i, band ->
            BandCard(i, band, onChange = { update(i, it) }) {
                onChange(Composite.formatBands(bands.toMutableList().also { it.removeAt(i) }))
            }
        }
        if (bands.size < Composite.MAX_BANDS) {
            OutlinedButton(
                onClick = { onChange(Composite.formatBands(bands + Composite.Band(0, 1000f, 0f, 1f))) },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Icon(Icons.Outlined.Add, contentDescription = null)
                Text("Add band")
            }
        }
    }
}

@Composable
private fun BandCard(index: Int, band: Composite.Band, onChange: (Composite.Band) -> Unit, onDelete: () -> Unit) {
    var typeOpen by remember { mutableStateOf(false) }
    // Gain does nothing for passes and notches; hide it there.
    val hasGain = band.type <= 2
    Card(
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHighest),
    ) {
        Column(Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Band ${index + 1}", Modifier.weight(1f), style = MaterialTheme.typography.titleSmall)
                Column {
                    TextButton(onClick = { typeOpen = true }) { Text(Composite.BAND_TYPES[band.type]) }
                    DropdownMenu(expanded = typeOpen, onDismissRequest = { typeOpen = false }) {
                        Composite.BAND_TYPES.forEachIndexed { t, name ->
                            DropdownMenuItem(text = { Text(name) }, onClick = { onChange(band.copy(type = t)); typeOpen = false })
                        }
                    }
                }
                IconButton(onClick = onDelete) { Icon(Icons.Outlined.Delete, contentDescription = "Remove band") }
            }
            // Frequency on a log scale: slider 0..1 spans 20 Hz..20 kHz.
            BandSlider(
                "Frequency", "${hzLabel(band.frequency.roundToInt().toFloat())} Hz",
                (ln(band.frequency / 20f) / ln(1000f)), 0f..1f,
            ) { onChange(band.copy(frequency = (20f * exp(it * ln(1000f))).roundToInt().toFloat())) }
            if (hasGain) {
                BandSlider("Gain", "%+.1f dB".format(band.gainDb), band.gainDb, -24f..24f) {
                    onChange(band.copy(gainDb = (it * 2).roundToInt() / 2f))
                }
            }
            BandSlider(if (band.type == 1 || band.type == 2) "Slope (Q)" else "Width (Q)", "%.2f".format(band.q), band.q, 0.1f..10f) {
                onChange(band.copy(q = (it * 20).roundToInt() / 20f))
            }
        }
    }
}

@Composable
private fun BandSlider(label: String, shown: String, value: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
    var local by remember(value) { mutableFloatStateOf(value) }
    Column {
        Row {
            Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
            Text(shown, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
        }
        Slider(value = local, onValueChange = { local = it; onChange(it) }, valueRange = range)
    }
}
