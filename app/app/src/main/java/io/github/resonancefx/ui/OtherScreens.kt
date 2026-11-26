// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.ui

import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.PowerManager
import android.provider.Settings as AndroidSettings
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.resonancefx.App
import io.github.resonancefx.BuildConfig
import io.github.resonancefx.R
import io.github.resonancefx.data.AttachMode
import io.github.resonancefx.driver.Diagnosis
import io.github.resonancefx.driver.DriverStatus
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.service.refreshMasterTile
import io.github.resonancefx.engine.TestTone
import io.github.resonancefx.localization.AppLanguage
import io.github.resonancefx.service.EngineService
import androidx.core.net.toUri

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
    var deleting by remember { mutableStateOf<String?>(null) }

    val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        runCatching { repo.importPreset(uri) }
            .onSuccess { Toast.makeText(context, context.getString(R.string.presets_imported, it), Toast.LENGTH_SHORT).show() }
            .onFailure { Toast.makeText(context, context.getString(R.string.presets_import_failed, it.message ?: ""), Toast.LENGTH_LONG).show() }
    }
    val exporter = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val name = exporting ?: return@rememberLauncherForActivityResult
        exporting = null
        uri ?: return@rememberLauncherForActivityResult
        runCatching { repo.exportPreset(name, uri) }
            .onFailure { Toast.makeText(context, context.getString(R.string.presets_export_failed, it.message ?: ""), Toast.LENGTH_LONG).show() }
    }

    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            ScreenTitle(stringResource(R.string.nav_presets))
            Text(
                stringResource(R.string.presets_intro, repo.activeProfileName),
                style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Row(Modifier.padding(vertical = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { saving = true }) { Text(stringResource(R.string.presets_save_current)) }
                OutlinedButton(onClick = { importer.launch(arrayOf("application/json", "*/*")) }) {
                    Icon(Icons.Outlined.FileDownload, null, Modifier.size(18.dp)); Spacer(Modifier.width(6.dp)); Text(stringResource(R.string.presets_import))
                }
            }
        }
        if (presets.isEmpty()) item {
            Text(stringResource(R.string.presets_empty), color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 24.dp))
        }
        items(presets, key = { it }) { name ->
            Card(shape = RoundedCornerShape(18.dp)) {
                ListItem(
                    headlineContent = { Text(name) },
                    modifier = Modifier.clickable {
                        repo.applyPreset(name)
                        Toast.makeText(context, context.getString(R.string.presets_applied, name), Toast.LENGTH_SHORT).show()
                    },
                    trailingContent = {
                        Row {
                            IconButton(onClick = { exporting = name; exporter.launch("$name.json") }) {
                                Icon(Icons.Outlined.FileUpload, contentDescription = stringResource(R.string.action_export))
                            }
                            IconButton(onClick = { deleting = name }) {
                                Icon(Icons.Outlined.Delete, contentDescription = stringResource(R.string.action_delete))
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
            title = { Text(stringResource(R.string.presets_save_title)) },
            text = {
                OutlinedTextField(
                    value = name, onValueChange = { name = it.take(48) },
                    label = { Text(stringResource(R.string.presets_name)) }, singleLine = true,
                )
            },
            confirmButton = {
                TextButton(enabled = name.isNotBlank(), onClick = { repo.savePreset(name.trim()); saving = false }) {
                    Text(stringResource(R.string.action_save))
                }
            },
            dismissButton = { TextButton(onClick = { saving = false }) { Text(stringResource(R.string.action_cancel)) } },
        )
    }

    deleting?.let { name ->
        AlertDialog(
            onDismissRequest = { deleting = null },
            title = { Text(stringResource(R.string.presets_delete_title)) },
            text = { Text(stringResource(R.string.presets_delete_confirm, name)) },
            confirmButton = {
                TextButton(onClick = {
                    repo.deletePreset(name)
                    deleting = null
                }) {
                    Text(stringResource(R.string.action_delete))
                }
            },
            dismissButton = {
                TextButton(onClick = { deleting = null }) {
                    Text(stringResource(R.string.action_cancel))
                }
            },
        )
    }
}

// ---- Status -----------------------------------------------------------------------------------

@Composable
fun StatusScreen() {
    val status = rememberDriverStatus()
    val context = LocalContext.current
    var showMeasurements by remember { mutableStateOf(false) }
    if (showMeasurements) MeasurementsDialog { showMeasurements = false }
    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { ScreenTitle(stringResource(R.string.title_engine_status)) }
        if (status == null) {
            item { Text(stringResource(R.string.status_querying)) }
            return@LazyColumn
        }
        item {
            Card(shape = RoundedCornerShape(22.dp), modifier = Modifier.fillMaxWidth()) {
                Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(stringResource(status.headlineRes), style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                    Check(stringResource(R.string.status_check_registered), status.registered, status.engineName)
                    Check(stringResource(R.string.status_check_attached), status.attached,
                        if (status.attached) null else stringResource(R.string.status_check_turn_on))
                    Check(
                        stringResource(R.string.status_check_stream), status.configured == true,
                        when {
                            status.disableReason != null -> status.disableReason
                            status.configured == null -> stringResource(R.string.status_check_no_stream)
                            else -> null
                        },
                    )
                    Check(stringResource(R.string.status_check_processing), status.processing ?: false,
                        if (status.processing == true) null else stringResource(R.string.status_check_play))
                    status.sessionError?.let { Check(stringResource(R.string.status_check_session), false, it) }
                    status.assetError?.let { Check(stringResource(R.string.status_check_imported_file), false, it) }
                    if (status.diagnosisCode != Diagnosis.OK.code) {
                        status.activeModuleDiagnosis?.let { module ->
                            Text(
                                listOf(module.short, module.fix).filter { it.isNotBlank() }.joinToString(" "),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        val appSide = status.diagnosisCode == Diagnosis.E12.code ||
                            status.diagnosisCode == Diagnosis.E13.code
                        Text(
                            stringResource(
                                if (appSide) R.string.status_diagnosis_app_hint else R.string.status_diagnosis_hint,
                                status.diagnosisCode,
                            ),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.error,
                        )
                        if (status.diagnosisCode == Diagnosis.E13.code) {
                            if (!status.masterOn) {
                                Text(
                                    stringResource(R.string.status_master_off),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            Button(
                                onClick = {
                                    if (!status.masterOn) {
                                        App.instance.repository.set(Catalog.KEY_MASTER, true)
                                    }
                                    EngineService.sync(context)
                                    refreshMasterTile(context)
                                },
                            ) { Text(stringResource(R.string.status_start_engine)) }
                        }
                    }
                }
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp), modifier = Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)) {
                Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Fact(stringResource(R.string.status_fact_driver), status.versionName?.let { "$it (${status.versionCode})" } ?: "—")
                    Fact(stringResource(R.string.status_fact_architecture), status.architecture ?: "—")
                    Fact(stringResource(R.string.status_fact_factory), status.factory ?: stringResource(R.string.status_fact_not_detected))
                    Fact(stringResource(R.string.status_fact_diagnosis), status.diagnosisCode)
                    Fact(stringResource(R.string.status_fact_sample_rate), status.sampleRate?.let { "$it Hz" } ?: "—")
                    Fact(stringResource(R.string.status_fact_channels), status.channels?.toString() ?: "—")
                    Fact(stringResource(R.string.status_fact_convolver), when (status.kernelLoaded) {
                        true -> stringResource(R.string.status_fact_kernel_loaded)
                        false -> stringResource(R.string.status_fact_no_kernel)
                        null -> "—"
                    })
                }
            }
        }
        item {
            // Equal weights keep the four actions the same size, whatever the language.
            Row(
                Modifier.fillMaxWidth().height(IntrinsicSize.Min),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                FilledTonalButton(
                    onClick = {
                        context.getSystemService(ClipboardManager::class.java)
                            .setPrimaryClip(ClipData.newPlainText("ResonanceFX status", status.report()))
                        Toast.makeText(context, context.getString(R.string.status_report_copied), Toast.LENGTH_SHORT).show()
                    },
                    modifier = Modifier.weight(1f).fillMaxHeight(),
                ) { Text(stringResource(R.string.status_copy_report)) }
                OutlinedButton(
                    onClick = { EngineService.start(context, EngineService.ACTION_REAPPLY) },
                    modifier = Modifier.weight(1f).fillMaxHeight(),
                ) { Text(stringResource(R.string.status_reapply)) }
            }
            Row(
                Modifier.fillMaxWidth().height(IntrinsicSize.Min).padding(top = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                OutlinedButton(onClick = { TestTone.play() }, modifier = Modifier.weight(1f).fillMaxHeight()) {
                    Text(stringResource(R.string.status_play_test_tone))
                }
                OutlinedButton(onClick = { showMeasurements = true }, modifier = Modifier.weight(1f).fillMaxHeight()) {
                    Text(stringResource(R.string.status_engine_measurements))
                }
            }
            Text(
                stringResource(R.string.status_test_tone_hint),
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
        if (!status.registered) item { Troubleshooting(status.factory) }
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
private fun Troubleshooting(factory: String? = null) {
    Card(shape = RoundedCornerShape(22.dp), colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer)) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Outlined.HelpOutline, null); Spacer(Modifier.width(8.dp))
                Text(stringResource(R.string.status_troubleshooting_title), style = MaterialTheme.typography.titleMedium)
            }
            Text(stringResource(R.string.status_troubleshooting_1))
            if (factory != null && factory.startsWith("AIDL")) {
                Text(stringResource(R.string.status_troubleshooting_aidl))
            }
            Text(stringResource(R.string.status_troubleshooting_2))
            Text(stringResource(R.string.status_troubleshooting_3))
            Text(stringResource(R.string.status_troubleshooting_4))
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
    var showLanguage by remember { mutableStateOf(false) }
    var language by remember { mutableStateOf(AppLanguage.current(context)) }

    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { ScreenTitle(stringResource(R.string.nav_settings)) }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    Text(
                        stringResource(R.string.settings_attach_to),
                        Modifier.padding(horizontal = 20.dp, vertical = 8.dp), style = MaterialTheme.typography.titleSmall,
                    )
                    ModeOption(
                        AttachMode.GLOBAL, mode,
                        stringResource(R.string.settings_mode_global_title), stringResource(R.string.settings_mode_global_body),
                    ) { mode = it }
                    ModeOption(
                        AttachMode.PER_APP, mode,
                        stringResource(R.string.settings_mode_per_app_title), stringResource(R.string.settings_mode_per_app_body),
                    ) { mode = it }
                }
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    SwitchRow(
                        stringResource(R.string.settings_per_device), stringResource(R.string.settings_per_device_body), perDevice,
                    ) { perDevice = it; repo.perDeviceProfiles = it }
                    SwitchRow(
                        stringResource(R.string.settings_start_on_boot), stringResource(R.string.settings_start_on_boot_body), boot,
                    ) { boot = it; repo.startOnBoot = it }
                }
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                ListItem(
                    headlineContent = { Text(stringResource(R.string.settings_language)) },
                    supportingContent = { Text(AppLanguage.label(context, language)) },
                    modifier = Modifier.clickable { showLanguage = true },
                )
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp)) {
                ListItem(
                    headlineContent = { Text(stringResource(R.string.settings_battery)) },
                    supportingContent = {
                        Text(
                            if (isIgnoringBattery(context)) stringResource(R.string.settings_battery_ok)
                            else stringResource(R.string.settings_battery_exempt),
                        )
                    },
                    modifier = Modifier.clickable { requestBatteryExemption(context) },
                )
            }
        }
        item {
            Card(shape = RoundedCornerShape(22.dp), colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)) {
                Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("ResonanceFX ${BuildConfig.VERSION_NAME}", style = MaterialTheme.typography.titleSmall)
                    Text(stringResource(R.string.settings_created_by), style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                    Text(
                        stringResource(R.string.settings_license_body),
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        stringResource(R.string.settings_autoeq_credit),
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    TextButton(onClick = { showLicenses = true }, contentPadding = PaddingValues(0.dp)) {
                        Text(stringResource(R.string.settings_open_source_licenses))
                    }
                    TextButton(onClick = { showMeasurements = true }, contentPadding = PaddingValues(0.dp)) {
                        Text(stringResource(R.string.status_engine_measurements))
                    }
                }
            }
        }
    }
    if (showLicenses) LicensesDialog { showLicenses = false }
    if (showMeasurements) MeasurementsDialog { showMeasurements = false }
    if (showLanguage) {
        LanguageDialog(language, onDismiss = { showLanguage = false }) { tag ->
            language = tag
            showLanguage = false
            AppLanguage.set(context, context as? Activity, tag)
        }
    }

    // Apply mode changes to the running service.
    androidx.compose.runtime.LaunchedEffect(mode) {
        if (repo.attachMode != mode) {
            repo.attachMode = mode
            if (EngineService.isRunning.value) EngineService.start(context, EngineService.ACTION_MODE_CHANGED)
        }
    }
}

@Composable
private fun LanguageDialog(current: String, onDismiss: () -> Unit, onPick: (String) -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.settings_language_dialog_title)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                (listOf("") + AppLanguage.SUPPORTED).forEach { tag ->
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .selectable(selected = tag == current, onClick = { onPick(tag) })
                            .padding(horizontal = 4.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        RadioButton(selected = tag == current, onClick = { onPick(tag) })
                        Text(AppLanguage.label(LocalContext.current, tag), Modifier.padding(start = 8.dp))
                    }
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_cancel)) } },
    )
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
        title = { Text(stringResource(R.string.settings_open_source_licenses)) },
        text = {
            LazyColumn {
                item {
                    Text("ResonanceFX", fontWeight = FontWeight.SemiBold)
                    Text(stringResource(R.string.licenses_body), style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.size(16.dp))
                    Text(stringResource(R.string.licenses_kemar_title), fontWeight = FontWeight.SemiBold)
                    Text(stringResource(R.string.licenses_kemar_body), style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.size(16.dp))
                    Text(stringResource(R.string.licenses_autoeq_title), fontWeight = FontWeight.SemiBold)
                    Text(autoEqLicense, style = MaterialTheme.typography.bodySmall)
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_close)) } },
    )
}
