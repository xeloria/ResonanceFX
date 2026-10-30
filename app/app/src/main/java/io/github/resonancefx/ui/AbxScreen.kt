// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import io.github.resonancefx.App
import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.Params
import io.github.resonancefx.model.Abx
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.service.EngineService
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.text.DateFormat
import java.util.Date
import kotlin.random.Random

/** What a test compares: the chain against the chain without [enableParam] (0 = all effects). */
private data class AbxTarget(val label: String, val enableParam: Int)

private sealed interface AbxState {
    data object Setup : AbxState
    data object Matching : AbxState
    data class Trial(val index: Int, val correct: Int, val xIsB: Boolean, val playing: Char) : AbxState
    data class Done(val result: Abx.Result) : AbxState
    data class Failed(val message: String) : AbxState
}

/**
 * Blind listening test: A is the reference (the effect off), B the chain as set, X secretly one of
 * them. The engine plays them time-aligned and loudness-matched, so only the effect itself can
 * give the answer away.
 */
@Composable
fun AbxDialog(settings: Settings, features: Int, onDismiss: () -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val targets = remember(settings, features) {
        listOf(AbxTarget("All effects", 0)) + Catalog.sections.filter { s ->
            s.enableKey != null && s.enableParam != null && settings.bool(s.enableKey) &&
                (s.feature == 0 || features and s.feature != 0)
        }.map { AbxTarget(it.title, it.enableParam!!) }
    }
    var target by remember { mutableStateOf(targets.first()) }
    var trials by remember { mutableIntStateOf(16) }
    var state by remember { mutableStateOf<AbxState>(AbxState.Setup) }
    var deltaDb by remember { mutableStateOf(0f) }
    var history by remember { mutableStateOf(Abx.history(context)) }
    val output = App.instance.repository.route.value.label

    fun play(which: Char, s: AbxState.Trial) {
        state = s.copy(playing = which)
        val chain = when (which) {
            'A' -> false
            'B' -> true
            else -> s.xIsB
        }
        scope.launch { EngineService.onEngine { it.abx(Params.ABX_SELECT, if (chain) 1 else 0) } }
    }

    fun start() {
        state = AbxState.Matching
        scope.launch {
            EngineService.onEngine {
                it.abx(Params.ABX_EXCLUDE, target.enableParam)
                it.abx(Params.ABX_SELECT, 1)
                it.abx(Params.ABX_MODE, 1)
            }
            // The engine needs 3 s of music to match the loudness of A and B.
            repeat(80) {
                delay(250)
                val d = EngineService.onEngine { it.abxDelta() }
                if (d != null && d != Params.ABX_NOT_READY) {
                    deltaDb = d / 100f
                    play('A', AbxState.Trial(0, 0, Random.nextBoolean(), 'A'))
                    return@launch
                }
            }
            state = AbxState.Failed("No music reached ResonanceFX. Play something in any app, then try again.")
        }
    }

    fun answer(s: AbxState.Trial, saysB: Boolean) {
        val correct = s.correct + if (saysB == s.xIsB) 1 else 0
        if (s.index + 1 < trials) {
            play('A', AbxState.Trial(s.index + 1, correct, Random.nextBoolean(), 'A'))
        } else {
            val r = Abx.Result(System.currentTimeMillis(), target.label, output, correct, trials)
            Abx.record(context, r)
            history = Abx.history(context)
            state = AbxState.Done(r)
            scope.launch { EngineService.onEngine { it.abx(Params.ABX_MODE, 0) } }
        }
    }

    // Whatever happens, leave the engine playing the chain normally.
    DisposableEffect(Unit) {
        onDispose {
            App.instance.applicationScope.launch {
                withContext(NonCancellable) {
                    EngineService.onEngine {
                        it.abx(Params.ABX_MODE, 0)
                        it.abx(Params.ABX_SELECT, 1)
                        it.abx(Params.ABX_EXCLUDE, 0)
                    }
                }
            }
        }
    }

    Dialog(onDismissRequest = onDismiss, properties = DialogProperties(dismissOnClickOutside = false)) {
        Surface(shape = RoundedCornerShape(28.dp), color = MaterialTheme.colorScheme.surfaceContainerHigh) {
            Column(
                Modifier.padding(24.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text("Blind test", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Medium)
                when (val s = state) {
                    AbxState.Setup -> {
                        Text(
                            "Can you really hear it? Play music in any app. A is without the effect, B with it, " +
                                "and X is secretly one of them. Both play at the same loudness, so only the " +
                                "effect itself can give X away.",
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        Text("Test", style = MaterialTheme.typography.titleSmall)
                        ChipRow(targets.map { it.label }, targets.indexOf(target)) { target = targets[it] }
                        Text("Trials", style = MaterialTheme.typography.titleSmall)
                        ChipRow(Abx.TRIAL_COUNTS.map { "$it" }, Abx.TRIAL_COUNTS.indexOf(trials)) { trials = Abx.TRIAL_COUNTS[it] }
                        Text(
                            "You'll need ${Abx.needed(trials)} of $trials right to show you hear a difference.",
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        if (history.isNotEmpty()) {
                            Text("Earlier results", style = MaterialTheme.typography.titleSmall)
                            history.take(5).forEach { HistoryRow(it) }
                        }
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = onDismiss) { Text("Close") }
                            Button(onClick = ::start) { Text("Start") }
                        }
                    }
                    AbxState.Matching -> {
                        Text("Matching loudness… keep the music playing.", style = MaterialTheme.typography.bodyLarge)
                        LinearProgressIndicator(Modifier.fillMaxWidth())
                    }
                    is AbxState.Trial -> {
                        Text("Trial ${s.index + 1} of $trials", style = MaterialTheme.typography.titleMedium)
                        LinearProgressIndicator(progress = { s.index / trials.toFloat() }, modifier = Modifier.fillMaxWidth())
                        Text(
                            levelNote(target.label, deltaDb),
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            for (which in listOf('A', 'B', 'X')) {
                                val modifier = Modifier.weight(1f).height(64.dp)
                                if (s.playing == which) {
                                    Button(onClick = {}, modifier = modifier) { Text("$which", style = MaterialTheme.typography.titleLarge) }
                                } else {
                                    OutlinedButton(onClick = { play(which, s) }, modifier = modifier) {
                                        Text("$which", style = MaterialTheme.typography.titleLarge)
                                    }
                                }
                            }
                        }
                        Text("Switch as often as you like, then answer.", style = MaterialTheme.typography.bodySmall)
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            FilledTonalButton(onClick = { answer(s, saysB = false) }, modifier = Modifier.weight(1f)) { Text("X is A") }
                            FilledTonalButton(onClick = { answer(s, saysB = true) }, modifier = Modifier.weight(1f)) { Text("X is B") }
                        }
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = onDismiss) { Text("Stop") }
                        }
                    }
                    is AbxState.Done -> {
                        val r = s.result
                        Text("${r.correct} of ${r.trials} correct", style = MaterialTheme.typography.headlineMedium)
                        Text(verdict(r), style = MaterialTheme.typography.bodyMedium)
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = { state = AbxState.Setup }) { Text("Test again") }
                            Button(onClick = onDismiss) { Text("Done") }
                        }
                    }
                    is AbxState.Failed -> {
                        Text(s.message, color = MaterialTheme.colorScheme.error)
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                            TextButton(onClick = onDismiss) { Text("Close") }
                            Button(onClick = { state = AbxState.Setup }) { Text("Back") }
                        }
                    }
                }
            }
        }
    }
}

private fun levelNote(target: String, deltaDb: Float): String {
    val what = if (target == "All effects") "The effects" else target
    return when {
        kotlin.math.abs(deltaDb) < 0.1f -> "A and B were already equally loud."
        deltaDb > 0 -> "$what made it ${"%.1f".format(deltaDb)} dB louder; B is turned down to match."
        else -> "$what made it ${"%.1f".format(-deltaDb)} dB quieter; A is turned down to match."
    }
}

private fun verdict(r: Abx.Result): String {
    val p = "%.3f".format(r.p)
    return if (r.heard) {
        "You hear a real difference. Guessing gets ${r.correct} or more right only ${chance(r.p)} (p = $p)."
    } else {
        "No reliable difference: guessing gets ${r.correct} or more right ${chance(r.p)} (p = $p). At equal " +
            "loudness this effect may not be audible to you with this music."
    }
}

private fun chance(p: Double): String = when {
    p >= 0.5 -> "about half the time or more"
    else -> "about 1 time in ${Math.round(1 / p)}"
}

@Composable
private fun ChipRow(labels: List<String>, selected: Int, onSelect: (Int) -> Unit) {
    // Wraps onto several rows for long effect lists.
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        labels.indices.chunked(3).forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { i -> FilterChip(selected = i == selected, onClick = { onSelect(i) }, label = { Text(labels[i]) }) }
            }
        }
    }
}

@Composable
private fun HistoryRow(r: Abx.Result) {
    Column {
        Text(
            "${r.target} · ${r.correct}/${r.trials} · ${if (r.heard) "heard" else "not heard"}",
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            "${r.output} · ${DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT).format(Date(r.time))}",
            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.height(4.dp))
    }
}
