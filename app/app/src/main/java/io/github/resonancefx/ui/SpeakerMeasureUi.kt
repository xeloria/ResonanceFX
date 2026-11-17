// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import android.Manifest
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.runtime.LaunchedEffect
import io.github.resonancefx.App
import io.github.resonancefx.data.Repository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.background
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.core.content.ContextCompat
import io.github.resonancefx.R
import io.github.resonancefx.measure.CorrectionFitter
import io.github.resonancefx.measure.MeasureException
import io.github.resonancefx.measure.SpeakerMeasurer
import io.github.resonancefx.model.Composite
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlin.math.log10
import kotlin.math.roundToInt

/** The measurement card in the speaker correction section. */
@Composable
fun MeasureRow(
    value: String,
    usingMeasured: Boolean,
    match: String,
    onSave: (String) -> Unit,
    onMatch: (reference: String, refitted: String) -> Unit,
    onUse: () -> Unit,
    onDelete: () -> Unit,
) {
    val measurement = remember(value) { Composite.parseMeasurement(value) }
    var measuring by remember { mutableStateOf(false) }
    var others by remember { mutableStateOf<List<Repository.MeasuredSpeaker>>(emptyList()) }
    val scope = rememberCoroutineScope()
    LaunchedEffect(value) {
        others = withContext(Dispatchers.IO) { App.instance.repository.otherMeasurements() }
    }
    val reference = others.firstOrNull { it.key == match }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(top = 4.dp)) {
        Text(stringResource(R.string.measure_your_speaker), style = MaterialTheme.typography.titleSmall)
        if (measurement == null) {
            Text(
                stringResource(R.string.measure_intro),
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Button(onClick = { measuring = true }, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.measure_start)) }
        } else {
            MeasurementSummary(measurement, reference)
            if (others.isNotEmpty()) {
                // Refit to another measured speaker's sound, or back to the natural target.
                SoundLike(others, match) { key ->
                    scope.launch {
                        val ref = others.firstOrNull { it.key == key }?.measurement
                        val refitted = withContext(Dispatchers.Default) { CorrectionFitter.refit(measurement, ref) }
                        onMatch(if (ref == null) "" else key, Composite.formatMeasurement(refitted))
                    }
                }
            }
            if (!usingMeasured) {
                Text(
                    stringResource(R.string.measure_pick_measured_above),
                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.primary,
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { measuring = true }) { Text(stringResource(R.string.measure_again)) }
                if (!usingMeasured) TextButton(onClick = onUse) { Text(stringResource(R.string.measure_use_it)) }
                Spacer(Modifier.weight(1f))
                TextButton(onClick = onDelete) { Text(stringResource(R.string.action_delete)) }
            }
        }
    }
    if (measuring) {
        MeasureDialog(onDismiss = { measuring = false }) {
            measuring = false
            onSave(Composite.formatMeasurement(it))
        }
    }
}

@Composable
private fun SoundLike(others: List<Repository.MeasuredSpeaker>, match: String, onPick: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    val current = others.firstOrNull { it.key == match }?.label ?: stringResource(R.string.measure_natural_target)
    Column {
        Text(stringResource(R.string.measure_sound_like), style = MaterialTheme.typography.bodyMedium)
        Box {
            OutlinedButton(onClick = { open = true }, modifier = Modifier.fillMaxWidth()) { Text(current) }
            DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
                DropdownMenuItem(text = { Text(stringResource(R.string.measure_natural_target)) }, onClick = { open = false; onPick("") })
                others.forEach { o ->
                    DropdownMenuItem(text = { Text(o.label) }, onClick = { open = false; onPick(o.key) })
                }
            }
        }
        Text(
            stringResource(R.string.measure_sound_like_hint),
            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun MeasurementSummary(m: Composite.Measurement, reference: Repository.MeasuredSpeaker? = null) {
    val correction = remember(m) { CorrectionFitter.correctionDb(m) }
    val corrected = remember(m) { m.responseDb.mapIndexed { i, v -> v + correction[i] } }
    val scheme = MaterialTheme.colorScheme
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        MeasurementGraph(
            listOfNotNull(
                reference?.let { it.measurement.responseDb to scheme.tertiary },
                m.responseDb to scheme.outline,
                corrected to scheme.primary,
            ),
            m.lowHz, m.highHz, Modifier.fillMaxWidth().height(150.dp),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Legend(scheme.outline, stringResource(R.string.measure_legend_measured))
            Legend(scheme.primary, stringResource(R.string.measure_legend_corrected))
            reference?.let { Legend(scheme.tertiary, it.label) }
        }
        val filters = pluralStringResource(R.plurals.measure_filters, m.bands.size, m.bands.size)
        Text(
            stringResource(R.string.measure_corrects, roundHz(m.lowHz), roundHz(m.highHz), filters) +
                if (m.preampDb < 0) stringResource(R.string.measure_headroom, io.github.resonancefx.model.Catalog.oneDecimal(m.preampDb)) else "",
            style = MaterialTheme.typography.bodySmall, color = scheme.onSurfaceVariant,
        )
        m.distortion?.let { DistortionSummary(it) }
    }
}

/** Where each band started to distort, from the optional distortion test. */
@Composable
private fun DistortionSummary(d: Composite.Distortion) {
    val names = listOf(
        R.string.measure_band_deep_bass, R.string.measure_band_bass, R.string.measure_band_low_mids,
        R.string.measure_band_mids, R.string.measure_band_upper_mids,
    )
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(stringResource(R.string.measure_distortion_test), style = MaterialTheme.typography.titleSmall)
        d.cleanUpToDb.forEachIndexed { i, level ->
            val band = if (i < names.size) stringResource(names[i]) else stringResource(R.string.measure_band_number, i + 1)
            val text = when {
                level == null -> stringResource(R.string.measure_distortion_clean)
                level < SpeakerMeasurer.DISTORTION_LEVELS.first() -> stringResource(R.string.measure_distortion_quiet)
                else -> stringResource(R.string.measure_distortion_up_to, "%.0f".format(java.util.Locale.ROOT, level))
            }
            Text(stringResource(R.string.measure_band_line, band, text), style = MaterialTheme.typography.bodySmall)
        }
        Text(
            stringResource(R.string.measure_distortion_hint),
            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

/** 312.7 -> "310", 14230 -> "14.2k". */
private fun roundHz(hz: Float) = hzLabel(if (hz >= 1000f) (hz / 100f).roundToInt() * 100f else (hz / 10f).roundToInt() * 10f)

@Composable
private fun Legend(color: Color, label: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Spacer(Modifier.size(10.dp).clip(CircleShape).background(color))
        Spacer(Modifier.width(6.dp))
        Text(label, style = MaterialTheme.typography.labelMedium)
    }
}

/** Response curves (dB on the 20 Hz-20 kHz log axis) with the corrected range shaded. */
@Composable
fun MeasurementGraph(curves: List<Pair<List<Float>, Color>>, lowHz: Float, highHz: Float, modifier: Modifier = Modifier) {
    val scheme = MaterialTheme.colorScheme
    Canvas(modifier.clip(RoundedCornerShape(16.dp))) {
        val range = 18f
        fun x(hz: Float) = size.width * (log10(hz / 20f) / 3f)
        fun y(db: Float) = size.height * (0.5f - db.coerceIn(-range, range) / (2 * range))
        drawRect(scheme.surfaceContainerHighest)
        drawRect(scheme.primary.copy(alpha = 0.06f), Offset(x(lowHz), 0f), androidx.compose.ui.geometry.Size(x(highHz) - x(lowHz), size.height))
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
            if (db.size < 2) return@forEach
            val path = Path()
            db.forEachIndexed { i, v ->
                val px = size.width * i / (db.size - 1)
                if (i == 0) path.moveTo(px, y(v)) else path.lineTo(px, y(v))
            }
            drawPath(path, color, style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round))
        }
    }
}

private sealed interface MeasureState {
    data object Intro : MeasureState
    data class Running(val progress: Float) : MeasureState
    data class Done(val measurement: Composite.Measurement) : MeasureState
    data class Failed(val message: String) : MeasureState
}

@Composable
private fun MeasureDialog(onDismiss: () -> Unit, onSave: (Composite.Measurement) -> Unit) {
    val context = LocalContext.current
    val measurer = remember { SpeakerMeasurer(context) }
    val scope = rememberCoroutineScope()
    var state by remember { mutableStateOf<MeasureState>(MeasureState.Intro) }
    var job by remember { mutableStateOf<Job?>(null) }
    var progress by remember { mutableFloatStateOf(0f) }
    var testDistortion by remember { mutableStateOf(false) }

    fun start() {
        state = MeasureState.Running(0f)
        job = scope.launch {
            state = try {
                MeasureState.Done(measurer.measure({ progress = it }, distortion = testDistortion))
            } catch (e: MeasureException) {
                MeasureState.Failed(e.message ?: context.getString(R.string.measure_failed))
            } catch (e: kotlinx.coroutines.CancellationException) {
                throw e
            } catch (e: Exception) {
                MeasureState.Failed(context.getString(R.string.measure_failed_detail, e.message ?: ""))
            }
        }
    }
    val permission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) start() else state = MeasureState.Failed(context.getString(R.string.measure_permission))
    }
    fun requestStart() {
        if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) start()
        else permission.launch(Manifest.permission.RECORD_AUDIO)
    }
    fun close() {
        job?.cancel()
        onDismiss()
    }

    Dialog(onDismissRequest = ::close, properties = DialogProperties(dismissOnClickOutside = false)) {
        Surface(shape = RoundedCornerShape(28.dp), color = MaterialTheme.colorScheme.surfaceContainerHigh) {
            Column(
                Modifier.padding(24.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text(stringResource(R.string.measure_start), style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Medium)
                when (val s = state) {
                    MeasureState.Intro -> {
                        val volume = (measurer.volume * 100).roundToInt()
                        Steps(
                            stringResource(R.string.measure_step_1),
                            stringResource(
                                R.string.measure_step_2, volume,
                                stringResource(if (volume < 30) R.string.measure_step_2_low else R.string.measure_step_2_ok),
                            ),
                            stringResource(R.string.measure_step_3),
                            stringResource(R.string.measure_step_4),
                        )
                        Text(
                            stringResource(R.string.measure_disclaimer),
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Checkbox(checked = testDistortion, onCheckedChange = { testDistortion = it })
                            Column {
                                Text(stringResource(R.string.measure_distortion_option), style = MaterialTheme.typography.bodyMedium)
                                Text(
                                    stringResource(R.string.measure_distortion_option_hint),
                                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = ::close) { Text(stringResource(R.string.action_cancel)) }
                            Button(onClick = ::requestStart) { Text(stringResource(R.string.action_start)) }
                        }
                    }
                    is MeasureState.Running -> {
                        Text(stringResource(R.string.measure_running), style = MaterialTheme.typography.bodyLarge)
                        LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = ::close) { Text(stringResource(R.string.action_cancel)) }
                        }
                    }
                    is MeasureState.Done -> {
                        MeasurementSummary(s.measurement)
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = ::requestStart) { Text(stringResource(R.string.measure_retry)) }
                            TextButton(onClick = ::close) { Text(stringResource(R.string.measure_discard)) }
                            Button(onClick = { onSave(s.measurement) }) { Text(stringResource(R.string.measure_save_and_use)) }
                        }
                    }
                    is MeasureState.Failed -> {
                        Text(s.message, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.error)
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = ::close) { Text(stringResource(R.string.action_close)) }
                            Button(onClick = ::requestStart) { Text(stringResource(R.string.measure_try_again)) }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun Steps(vararg steps: String) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        steps.forEachIndexed { i, step ->
            Row {
                Text("${i + 1}.", Modifier.width(24.dp), style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                Text(step, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}
