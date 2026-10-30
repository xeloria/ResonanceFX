// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.data

import android.content.Context
import android.net.Uri
import androidx.core.content.edit
import io.github.resonancefx.model.Catalog
import io.github.resonancefx.model.Composite
import io.github.resonancefx.model.FileControl
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONObject
import java.io.File

/** How effects attach to audio. */
enum class AttachMode {
    /** Session 0 (the global output mix): works with every app, deprecated but honored by most ROMs. */
    GLOBAL,

    /** Only sessions that players announce through OPEN_AUDIO_EFFECT_CONTROL_SESSION. */
    PER_APP,
}

/**
 * Source of truth for everything the UI edits and the service applies. Profiles are stored per
 * output route ("speaker", "wired", "bluetooth:<device>", ...) and switched automatically.
 */
class Repository(private val context: Context) {
    private val prefs = context.getSharedPreferences("app", Context.MODE_PRIVATE)
    private val profilesDir = File(context.filesDir, "profiles").apply { mkdirs() }
    private val presetsDir = File(context.filesDir, "presets").apply { mkdirs() }

    private val _route = MutableStateFlow(Route.SPEAKER)
    val route: StateFlow<Route> = _route.asStateFlow()

    private val _settings = MutableStateFlow(load(profileKey(Route.SPEAKER)))
    val settings: StateFlow<Settings> = _settings.asStateFlow()

    private val _presets = MutableStateFlow(listPresets())
    val presets: StateFlow<List<String>> = _presets.asStateFlow()

    var attachMode: AttachMode
        get() = AttachMode.valueOf(prefs.getString("attach_mode", AttachMode.GLOBAL.name)!!)
        set(value) = prefs.edit { putString("attach_mode", value.name) }

    var perDeviceProfiles: Boolean
        get() = prefs.getBoolean("per_device_profiles", true)
        set(value) {
            flushSave()
            prefs.edit { putBoolean("per_device_profiles", value) }
            _settings.value = load(profileKey(_route.value), _settings.value)
        }

    var startOnBoot: Boolean
        get() = prefs.getBoolean("start_on_boot", true)
        set(value) = prefs.edit { putBoolean("start_on_boot", value) }

    val activeProfileName: String get() = if (perDeviceProfiles) _route.value.label else "All outputs"

    fun onRouteChanged(route: Route) {
        rememberLabel(route)
        if (route == _route.value) return
        flushSave()
        _route.value = route
        _settings.value = load(profileKey(route), _settings.value)
    }

    fun update(transform: (Settings) -> Settings) {
        _settings.update(transform)
        scheduleSave()
    }

    // Slider drags produce many updates per second; write the profile once things settle.
    private val saveHandler = android.os.Handler(android.os.Looper.getMainLooper())
    private var savePending = false
    private val saveTask = Runnable {
        savePending = false
        save(profileKey(_route.value), _settings.value)
    }

    private fun scheduleSave() {
        savePending = true
        saveHandler.removeCallbacks(saveTask)
        saveHandler.postDelayed(saveTask, 400)
    }

    /** Writes any pending change immediately (before switching profiles). */
    private fun flushSave() {
        if (savePending) {
            saveHandler.removeCallbacks(saveTask)
            saveTask.run()
        }
    }

    fun set(key: String, value: Any) = update { it.with(key, value) }

    // ---- Presets ----------------------------------------------------------------------------

    fun savePreset(name: String) {
        File(presetsDir, fileName(name)).writeText(_settings.value.toJson().put("name", name).toString(2))
        _presets.value = listPresets()
    }

    fun applyPreset(name: String) {
        val json = runCatching { JSONObject(File(presetsDir, fileName(name)).readText()) }.getOrNull() ?: return
        update { Settings.fromJson(json) }
    }

    fun deletePreset(name: String) {
        File(presetsDir, fileName(name)).delete()
        _presets.value = listPresets()
    }

    fun exportPreset(name: String, target: Uri) {
        val text = File(presetsDir, fileName(name)).readText()
        context.contentResolver.openOutputStream(target, "wt")?.use { it.write(text.toByteArray()) }
    }

    /** Imports a ResonanceFX preset JSON. Returns the preset name. */
    fun importPreset(source: Uri): String {
        val text = context.contentResolver.openInputStream(source)?.use { it.readBytes().decodeToString() }
            ?: error("Can't read file")
        val json = JSONObject(text)
        val name = json.optString("name").ifBlank { displayName(source).substringBeforeLast('.') }
        Settings.fromJson(json) // validate
        File(presetsDir, fileName(name)).writeText(json.put("name", name).toString(2))
        _presets.value = listPresets()
        return name
    }

    private fun listPresets() = presetsDir.listFiles { f -> f.extension == "json" }.orEmpty()
        .mapNotNull { runCatching { JSONObject(it.readText()).optString("name") }.getOrNull()?.ifBlank { null } }
        .sortedBy { it.lowercase() }

    // ---- Speaker measurements of other outputs -------------------------------------------------

    /** A measured speaker in another profile, for "sound like". [key] identifies its profile. */
    data class MeasuredSpeaker(val key: String, val label: String, val measurement: Composite.Measurement)

    fun otherMeasurements(): List<MeasuredSpeaker> {
        val current = fileName(profileKey(_route.value))
        val labels = labels()
        return profilesDir.listFiles { f -> f.extension == "json" && f.name != current }.orEmpty().mapNotNull { file ->
            val settings = runCatching { Settings.fromJson(JSONObject(file.readText())) }.getOrNull() ?: return@mapNotNull null
            val m = Composite.parseMeasurement(settings.string(Catalog.KEY_SPEAKER_MEASURED)) ?: return@mapNotNull null
            MeasuredSpeaker(file.name, labels.optString(file.name, file.nameWithoutExtension), m)
        }.sortedBy { it.label.lowercase() }
    }

    fun measurementOf(key: String): Composite.Measurement? = otherMeasurements().firstOrNull { it.key == key }?.measurement

    private fun labels() = runCatching { JSONObject(prefs.getString("profile_labels", "{}")!!) }.getOrDefault(JSONObject())

    private fun rememberLabel(route: Route) {
        val labels = labels()
        val file = fileName(route.key)
        if (labels.optString(file) == route.label) return
        prefs.edit { putString("profile_labels", labels.put(file, route.label).toString()) }
    }

    // ---- Imported assets (DDC profiles, impulse responses) -----------------------------------

    fun assetDir(kind: FileControl.Kind) = File(context.filesDir, kind.directory).apply { mkdirs() }

    fun assetFile(kind: FileControl.Kind, name: String): File? =
        name.takeIf { it.isNotBlank() }?.let { File(assetDir(kind), it) }?.takeIf { it.isFile }

    fun listAssets(kind: FileControl.Kind): List<String> =
        assetDir(kind).listFiles().orEmpty().filter { it.isFile }.map { it.name }.sortedBy { it.lowercase() }

    /** Copies a picked file into app storage; validation happens in the caller. Returns its name. */
    fun importAsset(kind: FileControl.Kind, source: Uri): String {
        val name = displayName(source).replace(Regex("[^A-Za-z0-9._ ()-]"), "_")
        val dest = File(assetDir(kind), name)
        context.contentResolver.openInputStream(source)?.use { input -> dest.outputStream().use { input.copyTo(it) } }
            ?: error("Can't read file")
        return name
    }

    fun deleteAsset(kind: FileControl.Kind, name: String) {
        File(assetDir(kind), name).delete()
    }

    // ---- Storage ----------------------------------------------------------------------------

    private fun profileKey(route: Route) = if (perDeviceProfiles) route.key else "all"

    /** A route seen for the first time starts from [fallback] (the last-used profile). */
    private fun load(profile: String, fallback: Settings = Settings.DEFAULT): Settings {
        val file = File(profilesDir, fileName(profile))
        return runCatching { Settings.fromJson(JSONObject(file.readText())) }.getOrNull() ?: fallback
    }

    private fun save(profile: String, settings: Settings) {
        val file = File(profilesDir, fileName(profile))
        val tmp = File(profilesDir, "${file.name}.tmp")
        tmp.writeText(settings.toJson().toString(2))
        tmp.renameTo(file)
    }

    private fun fileName(name: String) = name.replace(Regex("[^A-Za-z0-9._-]"), "_") + ".json"

    private fun displayName(uri: Uri): String {
        context.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { if (it.moveToFirst()) return it.getString(0) }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "imported"
    }
}
