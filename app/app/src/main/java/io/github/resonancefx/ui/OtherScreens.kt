// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import android.os.Build
import android.os.PowerManager
import android.provider.Settings as AndroidSettings
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.ErrorOutline
import androidx.compose.material.icons.outlined.FileDownload
import androidx.compose.material.icons.outlined.FileUpload
import androidx.compose.material.icons.outlined.HelpOutline
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.resonancefx.App
import io.github.resonancefx.BuildConfig
import io.github.resonancefx.data.AttachMode
import io.github.resonancefx.driver.DriverStatus
import io.github.resonancefx.engine.TestTone
import io.github.resonancefx.service.EngineService

@Composable
private fun ScreenTitle(text: String) {
    Text(text, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold, modifier = Modifier.padding(bottom = 4.dp))
}

// ---- Presets ----------------------------------------------------------------------------------

@Composable
fun PresetsScreen() {
    val repo = App.instance.repository
    val context = LocalContext.current
    val presets by repo.presets.collectAsStateWithLifecycle()
    var saving by remember { mutableStateOf(false) }
    var exporting by remember { mutableStateOf<String?>(null) }

    val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        runCatching { repo.importPreset(uri) }
            .onSuccess { Toast.makeText(context, "Imported \"$it\"", Toast.LENGTH_SHORT).show() }
            .onFailure { Toast.makeText(context, "Not a ResonanceFX preset: ${it.message}", Toast.LENGTH_LONG).show() }
    }
    val exporter = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val name = exporting ?: return@rememberLauncherForActivityResult
        exporting = null
        uri ?: return@rememberLauncherForActivityResult
        runCatching { repo.exportPreset(name, uri) }
            .onFailure { Toast.makeText(context, "Export failed: ${it.message}", Toast.LENGTH_LONG).show() }
    }

    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            ScreenTitle("Presets")
            Text(
                "Presets capture every effect setting. Applying one replaces the profile for ${repo.activeProfileName}.",
                style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Row(Modifier.padding(vertical = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { saving = true }) { Text("Save current") }
                OutlinedButton(onClick = { importer.launch(arrayOf("application/json", "*/*")) }) {
                    Icon(Icons.Outlined.FileDownload, null, Modifier.size(18.dp)); Spacer(Modifier.width(6.dp)); Text("Import")
                }
            }
        }
        if (presets.isEmpty()) item {
            Text("No presets yet.", color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 24.dp))
        }
        items(presets, key = { it }) { name ->
            Card(shape = RoundedCornerShape(18.dp)) {
                ListItem(
                    headlineContent = { Text(name) },
                    modifier = Modifier.clickable {
                        repo.applyPreset(name)
                        Toast.makeText(context, "Applied \"$name\"", Toast.LENGTH_SHORT).show()
                    },
                    trailingContent = {
                        Row {
                            IconButton(onClick = { exporting = name; exporter.launch("$name.json") }) {
                                Icon(Icons.Outlined.FileUpload, contentDescription = "Export")
                            }
                            IconButton(onClick = { repo.deletePreset(name) }) {
                                Icon(Icons.Outlined.Delete, contentDescription = "Delete")
                            }
                        }
                    },
                )
            }
        }
    }

    if (saving) {
        var name by remember { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { saving = false },
            title = { Text("Save preset") },
            text = { OutlinedTextField(value = name, onValueChange = { name = it.take(48) }, label = { Text("Name") }, singleLine = true) },
            confirmButton = {
                TextButton(enabled = name.isNotBlank(), onClick = { repo.savePreset(name.trim()); saving = false }) { Text("Save") }
            },
            dismissButton = { TextButton(onClick = { saving = false }) { Text("Cancel") } },
        )
    }
}

// ---- Status -----------------------------------------------------------------------------------

@Composable
fun StatusScreen() {
    val status = rememberDriverStatus(1500)
    val context = LocalContext.current
    var showMeasurements by remember { mutableStateOf(false) }
    if (showMeasurements) MeasurementsDialog { showMeasurements = false }
    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { ScreenTitle("Engine status") }
        if (status == null) {
            item { Text("Querying…") }
            return@LazyColumn
        }
        item {
            Card(shape = RoundedCornerShape(22.dp), modifier = Modifier.fillMaxWidth()) {
                Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(status.headline, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                    Check("Engine registered with Android", status.registered, status.engineName)
                    Check("Attached to audio", status.attached, if (status.attached) null else "Turn effects on")
                    Check(
                        "Audio stream accepted", status.configured == true,
                        when {
                            status.disableReason != null -> status.disableReason
                            status.configured == null -> "No stream yet"
                            else -> null
                        },
                    )
                    Check("Processing audio", status.processing ?: false, if (status.processing == true) null else "Play something")
                    status.sessionError?.let { Check("Session", false, it) }
                    status.assetError?.let { Check("Imported file", false, it) }
                }
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp), modifier = Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)) {
                Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Fact("Driver", status.versionName?.let { "$it (${status.versionCode})" } ?: "—")
                    Fact("Architecture", status.architecture ?: "—")
                    Fact("Sample rate", status.sampleRate?.let { "$it Hz" } ?: "—")
                    Fact("Channels", status.channels?.toString() ?: "—")
                    Fact("Convolver", when (status.kernelLoaded) { true -> "Impulse response loaded"; false -> "No impulse response"; null -> "—" })

                }
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilledTonalButton(onClick = {
                    context.getSystemService(ClipboardManager::class.java)
                        .setPrimaryClip(ClipData.newPlainText("ResonanceFX status", status.report()))
                    Toast.makeText(context, "Report copied", Toast.LENGTH_SHORT).show()
                }) { Text("Copy report") }
                OutlinedButton(onClick = { EngineService.start(context, EngineService.ACTION_REAPPLY) }) { Text("Re-apply") }
            }
            Row(Modifier.padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { TestTone.play() }) { Text("Play test tone") }
                OutlinedButton(onClick = { showMeasurements = true }) { Text("Engine measurements") }
            }
            Text(
                "Plays a 3-second chord. With effects on, \"Processing audio\" should get a check mark while it plays.",
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
        if (!status.registered) item { Troubleshooting() }
    }
}

@Composable
private fun Check(label: String, ok: Boolean, detail: String?) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(
            if (ok) Icons.Outlined.CheckCircle else Icons.Outlined.ErrorOutline, contentDescription = null,
            tint = if (ok) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error,
        )
        Spacer(Modifier.width(12.dp))
        Column {
            Text(label, style = MaterialTheme.typography.bodyLarge)
            detail?.let { Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant) }
        }
    }
}

@Composable
private fun Fact(label: String, value: String) {
    Row(Modifier.fillMaxWidth()) {
        Text(label, Modifier.weight(0.4f), color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, Modifier.weight(0.6f), fontWeight = FontWeight.Medium, textAlign = androidx.compose.ui.text.style.TextAlign.End)
    }
}

@Composable
private fun Troubleshooting() {
    Card(shape = RoundedCornerShape(22.dp), colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer)) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Outlined.HelpOutline, null); Spacer(Modifier.width(8.dp))
                Text("The engine isn't loaded", style = MaterialTheme.typography.titleMedium)
            }
            Text("1. Flash ResonanceFX-<version>.zip in Magisk, KernelSU or APatch and reboot.")
            Text("2. In your root manager, tap the module's Action button: it re-patches, re-mounts and restarts audio, then reports what it found.")
            Text("3. Remove other audio-effect modules that install the same engine — the installer disables them, but leftovers in /vendor can conflict.")
            Text("4. Still failing? Copy the report above together with /data/adb/resonancefx/logs.")
        }
    }
}

// ---- Settings ---------------------------------------------------------------------------------

@Composable
fun SettingsScreen() {
    val repo = App.instance.repository
    val context = LocalContext.current
    var mode by remember { mutableStateOf(repo.attachMode) }
    var perDevice by remember { mutableStateOf(repo.perDeviceProfiles) }
    var boot by remember { mutableStateOf(repo.startOnBoot) }
    var showLicenses by remember { mutableStateOf(false) }
    var showMeasurements by remember { mutableStateOf(false) }

    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { ScreenTitle("Settings") }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    Text("Attach to", Modifier.padding(horizontal = 20.dp, vertical = 8.dp), style = MaterialTheme.typography.titleSmall)
                    ModeOption(AttachMode.GLOBAL, mode, "All audio (recommended)", "Processes the global output mix. Works with every app.") { mode = it }
                    ModeOption(AttachMode.PER_APP, mode, "Per-app sessions", "Only players that announce their audio session (most music apps).") { mode = it }
                }
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    SwitchRow("Per-device profiles", "Separate settings for speaker, wired, each Bluetooth device and USB DAC.", perDevice) {
                        perDevice = it; repo.perDeviceProfiles = it
                    }
                    SwitchRow("Start on boot", "Re-attach automatically after a reboot.", boot) { boot = it; repo.startOnBoot = it }
                }
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                ListItem(
                    headlineContent = { Text("Battery optimization") },
                    supportingContent = { Text(if (isIgnoringBattery(context)) "Unrestricted — effects won't be killed" else "Tap to exempt ResonanceFX so effects aren't stopped in the background") },
                    modifier = Modifier.clickable { requestBatteryExemption(context) },
                )
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp), colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)) {
                Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("ResonanceFX ${BuildConfig.VERSION_NAME}", style = MaterialTheme.typography.titleSmall)
                    Text("Created by Xeloria", style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                    Text(
                        "Free and open-source software, licensed under the GPL-3.0. " +
                            "No ads, no analytics, no tracking and no network access — your audio settings never leave your device.",
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        "Headphone profiles from the AutoEq project by Jaakko Pasanen (MIT license).",
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    TextButton(onClick = { showLicenses = true }, contentPadding = PaddingValues(0.dp)) { Text("Open-source licenses") }
                    TextButton(onClick = { showMeasurements = true }, contentPadding = PaddingValues(0.dp)) { Text("Engine measurements") }
                }
            }
        }
    }
    if (showLicenses) LicensesDialog { showLicenses = false }
    if (showMeasurements) MeasurementsDialog { showMeasurements = false }

    // Apply mode changes to the running service.
    androidx.compose.runtime.LaunchedEffect(mode) {
        if (repo.attachMode != mode) {
            repo.attachMode = mode
            if (EngineService.isRunning.value) EngineService.start(context, EngineService.ACTION_MODE_CHANGED)
        }
    }
}

@Composable
private fun ModeOption(value: AttachMode, current: AttachMode, title: String, body: String, onSelect: (AttachMode) -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .selectable(selected = value == current, onClick = { onSelect(value) })
            .padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(selected = value == current, onClick = { onSelect(value) })
        Column(Modifier.padding(start = 8.dp)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            Text(body, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun SwitchRow(title: String, body: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    ListItem(
        headlineContent = { Text(title) },
        supportingContent = { Text(body) },
        trailingContent = { Switch(checked = checked, onCheckedChange = onChange) },
        modifier = Modifier.clickable { onChange(!checked) },
    )
}

private fun isIgnoringBattery(context: Context) =
    context.getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(context.packageName)

private fun requestBatteryExemption(context: Context) {
    if (isIgnoringBattery(context)) return
    runCatching {
        context.startActivity(
            Intent(AndroidSettings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, "package:${context.packageName}".toUri()),
        )
    }.onFailure {
        context.startActivity(Intent(AndroidSettings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
    }
}

@Composable
private fun LicensesDialog(onDismiss: () -> Unit) {
    val context = LocalContext.current
    val autoEqLicense = remember {
        runCatching { context.assets.open("autoeq/LICENSE").use { it.readBytes().decodeToString() } }.getOrDefault("")
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Open-source licenses") },
        text = {
            LazyColumn {
                item {
                    Text("ResonanceFX", fontWeight = FontWeight.SemiBold)
                    Text(
                        "Copyright (C) 2026 Xeloria. Licensed under the GNU General Public License, version 3 or later. " +
                            "The DSP core comes from Basilar, also by Xeloria (GPL-3.0-or-later). " +
                            "The source code, including the NOTICE file with third-party attributions, is part of the project repository.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    Spacer(Modifier.size(16.dp))
                    Text("MIT KEMAR HRTF measurements", fontWeight = FontWeight.SemiBold)
                    Text(
                        "Spatial audio uses head-related impulse responses from W. G. Gardner and K. D. Martin, " +
                            "\"HRTF Measurements of a KEMAR Dummy-Head Microphone\", MIT Media Lab Perceptual Computing " +
                            "Technical Report #280, 1994.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    Spacer(Modifier.size(16.dp))
                    Text("AutoEq headphone data", fontWeight = FontWeight.SemiBold)
                    Text(autoEqLicense, style = MaterialTheme.typography.bodySmall)
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } },
    )
}
