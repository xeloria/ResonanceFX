// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.FileOpen
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import io.github.resonancefx.App
import io.github.resonancefx.engine.Applier
import io.github.resonancefx.engine.Assets
import io.github.resonancefx.model.FileControl
import io.github.resonancefx.model.Tables
import kotlin.math.abs
import kotlin.math.roundToInt

private const val EQ_RANGE_DB = 12f

/** The 10-band FIR equalizer: presets plus an [EqCurve]. */
@Composable
fun EqualizerControl(value: String, onChange: (String) -> Unit) {
    val gains = remember(value) { Applier.parseBands(value) }
    var presetsOpen by remember { mutableStateOf(false) }
    val presetName = Tables.EQ_PRESETS.firstOrNull { (_, v) -> Applier.parseBands(v).contentEquals(gains) }?.first ?: "Custom"

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth()) {
            Text("Preset", Modifier.weight(1f).padding(top = 12.dp), style = MaterialTheme.typography.bodyMedium)
            Column {
                TextButton(onClick = { presetsOpen = true }) { Text(presetName) }
                DropdownMenu(expanded = presetsOpen, onDismissRequest = { presetsOpen = false }) {
                    Tables.EQ_PRESETS.forEach { (name, bands) ->
                        DropdownMenuItem(text = { Text(name) }, onClick = { onChange(bands); presetsOpen = false })
                    }
                }
            }
        }
        EqCurve(gains, Tables.EQ_BANDS_HZ.map { it.toFloat() }) { onChange(Applier.formatBands(it)) }
    }
}

/** Short frequency label: 31.5, 250, 1k, 12.5k. */
fun hzLabel(hz: Float): String = when {
    hz >= 1000f -> if (hz % 1000f == 0f) "${(hz / 1000f).toInt()}k" else "%.1fk".format(hz / 1000f)
    hz % 1f == 0f -> "${hz.toInt()}"
    else -> "%.1f".format(hz)
}

/**
 * Drag a band vertically to set its gain (0.5 dB steps); double-tap a band to reset it.
 * The curve is a Catmull-Rom spline through the band points, filled with a gradient.
 * With many bands only every few labels are shown; the dragged band's value is always shown.
 */
@Composable
fun EqCurve(gains: FloatArray, bandsHz: List<Float>, onChange: (FloatArray) -> Unit) {
    var dragBand by remember { mutableIntStateOf(-1) }
    val scheme = MaterialTheme.colorScheme
    val key = gains.joinToString()

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Canvas(
            Modifier
                .fillMaxWidth()
                .height(180.dp)
                .clip(RoundedCornerShape(16.dp))
                .pointerInput(key) {
                    fun bandAt(x: Float) = ((x / size.width) * gains.size).toInt().coerceIn(0, gains.size - 1)
                    fun gainAt(y: Float) = (((0.5f - y / size.height) * 2 * EQ_RANGE_DB) * 2).roundToInt() / 2f
                    detectDragGestures(
                        onDragStart = { dragBand = bandAt(it.x) },
                        onDragEnd = { dragBand = -1 },
                        onDragCancel = { dragBand = -1 },
                    ) { change, _ ->
                        val band = dragBand.takeIf { it >= 0 } ?: return@detectDragGestures
                        val g = gainAt(change.position.y).coerceIn(-EQ_RANGE_DB, EQ_RANGE_DB)
                        if (abs(g - gains[band]) >= 0.5f) {
                            gains[band] = g
                            onChange(gains.copyOf())
                        }
                    }
                }
                .pointerInput(key) {
                    detectTapGestures(onDoubleTap = {
                        val band = ((it.x / size.width) * gains.size).toInt().coerceIn(0, gains.size - 1)
                        gains[band] = 0f
                        onChange(gains.copyOf())
                    })
                },
        ) {
            val w = size.width
            val h = size.height
            drawRect(scheme.surfaceContainerHighest)
            // Grid: 0 dB and +-6 dB.
            listOf(-6f, 0f, 6f).forEach { db ->
                val y = h * (0.5f - db / (2 * EQ_RANGE_DB))
                drawLine(
                    scheme.outlineVariant, Offset(0f, y), Offset(w, y), strokeWidth = 1.dp.toPx(),
                    pathEffect = if (db == 0f) null else PathEffect.dashPathEffect(floatArrayOf(8f, 8f)),
                )
            }
            val points = gains.mapIndexed { i, g ->
                Offset(w * (i + 0.5f) / gains.size, h * (0.5f - g / (2 * EQ_RANGE_DB)))
            }
            val curve = Path().apply {
                moveTo(0f, points.first().y)
                lineTo(points.first().x, points.first().y)
                for (i in 0 until points.size - 1) {
                    val p0 = points[maxOf(i - 1, 0)]
                    val p1 = points[i]
                    val p2 = points[i + 1]
                    val p3 = points[minOf(i + 2, points.size - 1)]
                    cubicTo(
                        p1.x + (p2.x - p0.x) / 6f, p1.y + (p2.y - p0.y) / 6f,
                        p2.x - (p3.x - p1.x) / 6f, p2.y - (p3.y - p1.y) / 6f,
                        p2.x, p2.y,
                    )
                }
                lineTo(w, points.last().y)
            }
            val fill = Path().apply {
                addPath(curve)
                lineTo(w, h)
                lineTo(0f, h)
                close()
            }
            drawPath(fill, Brush.verticalGradient(listOf(scheme.primary.copy(alpha = 0.45f), scheme.primary.copy(alpha = 0.02f))))
            drawPath(curve, scheme.primary, style = Stroke(width = 3.dp.toPx(), cap = StrokeCap.Round))
            val dot = if (gains.size > 15) 3.dp.toPx() else 5.dp.toPx()
            points.forEachIndexed { i, p ->
                drawCircle(if (i == dragBand) scheme.secondary else scheme.onSurface, radius = if (i == dragBand) 9.dp.toPx() else dot, center = p)
            }
        }
        if (gains.size <= 10) {
            Row(Modifier.fillMaxWidth()) {
                bandsHz.forEachIndexed { i, hz ->
                    Column(Modifier.weight(1f)) {
                        Text(hzLabel(hz), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        Text(
                            "%+.1f".format(gains[i]),
                            style = MaterialTheme.typography.labelSmall,
                            color = if (i == dragBand) MaterialTheme.colorScheme.secondary else MaterialTheme.colorScheme.primary,
                        )
                    }
                }
            }
        } else {
            val step = if (gains.size > 15) 5 else 3
            // Each label spans its whole group of bands, so it has room to show in full.
            Row(Modifier.fillMaxWidth()) {
                // A short final group (31 = 6 x 5 + 1) joins the one before it.
                val starts = (bandsHz.indices step step).filter { bandsHz.size - it >= step || it == 0 }
                starts.forEachIndexed { g, i ->
                    val end = starts.getOrElse(g + 1) { bandsHz.size }
                    Text(
                        hzLabel(bandsHz[i]),
                        Modifier.weight((end - i).toFloat()),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        maxLines = 1,
                        softWrap = false,
                    )
                }
            }
            Text(
                if (dragBand >= 0) "${hzLabel(bandsHz[dragBand])} Hz  %+.1f dB".format(gains[dragBand])
                else "Drag a band to adjust it, double-tap to reset it",
                style = MaterialTheme.typography.labelMedium,
                color = if (dragBand >= 0) MaterialTheme.colorScheme.secondary else MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

/** Pick an imported file, import a new one (validated before it's accepted), or delete one. */
@Composable
fun FileRow(control: FileControl, value: String, onChange: (String) -> Unit) {
    val repo = App.instance.repository
    val context = LocalContext.current
    var menuOpen by remember { mutableStateOf(false) }
    var refresh by remember { mutableIntStateOf(0) }
    val files = remember(refresh) { repo.listAssets(control.kind) }

    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val name = runCatching { repo.importAsset(control.kind, uri) }.getOrElse {
            Toast.makeText(context, "Import failed: ${it.message}", Toast.LENGTH_LONG).show()
            return@rememberLauncherForActivityResult
        }
        val error = runCatching {
            val file = repo.assetFile(control.kind, name)!!
            when (control.kind) {
                FileControl.Kind.DDC -> Assets.parseVdc(file.readText())
                FileControl.Kind.KERNEL -> Assets.parseWav(file.readBytes())
            }
        }.exceptionOrNull()
        if (error != null) {
            repo.deleteAsset(control.kind, name)
            Toast.makeText(context, "Not a valid file: ${error.message}", Toast.LENGTH_LONG).show()
        } else {
            onChange(name)
        }
        refresh++
    }

    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Column(Modifier.weight(1f)) {
            Text(control.label, style = MaterialTheme.typography.bodyMedium)
            OutlinedButton(onClick = { menuOpen = true }, modifier = Modifier.fillMaxWidth()) {
                Text(value.ifBlank { "None" }, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
                DropdownMenuItem(text = { Text("None") }, onClick = { onChange(""); menuOpen = false })
                files.forEach { f ->
                    DropdownMenuItem(
                        text = { Text(f) },
                        onClick = { onChange(f); menuOpen = false },
                        trailingIcon = {
                            IconButton(onClick = {
                                repo.deleteAsset(control.kind, f)
                                if (value == f) onChange("")
                                refresh++
                            }) { Icon(Icons.Outlined.Delete, contentDescription = "Delete") }
                        },
                    )
                }
            }
        }
        IconButton(onClick = { picker.launch(arrayOf("*/*")) }, modifier = Modifier.padding(top = 20.dp)) {
            Icon(Icons.Outlined.FileOpen, contentDescription = "Import ${control.kind.extensions.joinToString("/")}")
        }
    }
}
