// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.data

import android.content.Context
import android.net.Uri
import android.util.Log
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
        val file = File(presetsDir, fileName(name))
        file.writeText(_settings.value.toJson().put("name", name).toString(2))
        val legacy = File(presetsDir, legacyFileName(name))
        if (legacy.isFile && legacy.name != file.name) legacy.delete()
        _presets.value = listPresets()
    }

    fun applyPreset(name: String) {
        val json = runCatching { JSONObject(presetFile(name).readText()) }.getOrNull() ?: return
        update { Settings.fromJson(json) }
    }

    fun deletePreset(name: String) {
        File(presetsDir, legacyFileName(name)).delete()
        presetFile(name).delete()
        _presets.value = listPresets()
    }

    fun exportPreset(name: String, target: Uri) {
        val text = presetFile(name).readText()
        context.contentResolver.openOutputStream(target, "wt")?.use { it.write(text.toByteArray()) }
    }

    /** Imports a ResonanceFX preset JSON. Returns the preset name. */
    fun importPreset(source: Uri): String {
        val text = context.contentResolver.openInputStream(source)?.use { it.readBytes().decodeToString() }
            ?: error("Can't read file")
        val json = JSONObject(text)
        val name = json.optString("name").ifBlank { displayName(source).substringBeforeLast('.') }
        Settings.fromJson(json) // validate
        val file = File(presetsDir, fileName(name))
        file.writeText(json.put("name", name).toString(2))
        val legacy = File(presetsDir, legacyFileName(name))
        if (legacy.isFile && legacy.name != file.name) legacy.delete()
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
        val current = profileFile(profileKey(_route.value)).name
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
        assetDir(kind).listFiles().orEmpty().filter { it.isFile && !it.name.endsWith(STAGED_SUFFIX) }
            .map { it.name }.sortedBy { it.lowercase() }

    /**
     * Copies a picked file into a uniquely staged file; the caller parses it and then either
     * publishes it with [commitAsset] or throws it away with [discardAsset]. A failed import can
     * therefore never overwrite an existing, valid asset of the same name.
     */
    fun stageAsset(kind: FileControl.Kind, source: Uri): File {
        val dir = assetDir(kind)
        // Clean up files staged by a crashed attempt more than a day ago.
        dir.listFiles()?.filter { it.name.endsWith(STAGED_SUFFIX) && it.lastModified() < System.currentTimeMillis() - 86_400_000 }
            ?.forEach { it.delete() }
        val base = displayName(source).replace(Regex("[^A-Za-z0-9._ ()-]"), "_")
        var staged = File(dir, "$base$STAGED_SUFFIX")
        var i = 1
        while (staged.exists()) staged = File(dir, "$base(${++i})$STAGED_SUFFIX")
        context.contentResolver.openInputStream(source)?.use { input -> staged.outputStream().use { input.copyTo(it) } }
            ?: error("Can't read file")
        return staged
    }

    /** Publishes a staged file under its final name, replacing any earlier asset of that name. */
    fun commitAsset(kind: FileControl.Kind, staged: File): String {
        val name = staged.name.removeSuffix(STAGED_SUFFIX)
        val dest = File(assetDir(kind), name)
        if (!staged.renameTo(dest)) staged.copyTo(dest, overwrite = true).let { staged.delete() }
        return name
    }

    /** Throws away an asset staged by [stageAsset], e.g. because parsing failed. */
    fun discardAsset(staged: File) {
        staged.delete()
    }

    fun deleteAsset(kind: FileControl.Kind, name: String) {
        File(assetDir(kind), name).delete()
    }

    // ---- Storage ----------------------------------------------------------------------------

    private fun profileKey(route: Route) = if (perDeviceProfiles) route.key else "all"

    /** A route seen for the first time starts from [fallback] (the last-used profile). */
    private fun load(profile: String, fallback: Settings = Settings.DEFAULT): Settings {
        val file = profileFile(profile)
        return runCatching { Settings.fromJson(JSONObject(file.readText())) }.getOrNull() ?: fallback
    }

    private fun save(profile: String, settings: Settings) {
        val json = settings.toJson().toString(2)
        val file = File(profilesDir, fileName(profile))
        val tmp = File(profilesDir, "${file.name}.tmp")
        try {
            tmp.writeText(json)
            // Rename is atomic, but it can fail (backup/AV scanners holding the target); fall
            // back to an in-place write instead of silently losing the profile.
            if (!tmp.renameTo(file)) {
                file.writeText(json)
                tmp.delete()
            }
            // Migrated: the legacy-named profile is superseded by the hashed one.
            val legacy = File(profilesDir, legacyFileName(profile))
            if (legacy.isFile && legacy.name != file.name) legacy.delete()
        } catch (e: Exception) {
            Log.w(TAG, "Can't save profile \"$profile\"", e)
            tmp.delete()
        }
    }

    private fun legacyFileName(name: String) = name.replace(Regex("[^A-Za-z0-9._-]"), "_") + ".json"

    /**
     * The old naming sanitized every profile/preset name, so different outputs ("Galaxy Buds
     * Pro" and "Galaxy-Buds-Pro") mapped onto one file and silently shared a profile. New
     * writes use the exact name (a short hash appended when sanitization actually lost
     * characters); reading accepts the legacy file until then, so nothing is lost on update.
     */
    private fun fileName(name: String): String {
        val clean = name.replace(Regex("[^A-Za-z0-9._-]"), "_")
        if (clean == name) return "$clean.json"
        val digest = java.security.MessageDigest.getInstance("SHA-1").digest(name.toByteArray(Charsets.UTF_8))
        val hex = digest.take(3).joinToString("") { "%02x".format(java.util.Locale.ROOT, it) }
        return "$clean-$hex.json"
    }

    private fun profileFile(profile: String): File {
        val file = File(profilesDir, fileName(profile))
        if (file.isFile) return file
        return File(profilesDir, legacyFileName(profile)).takeIf { it.isFile } ?: file
    }

    private fun presetFile(name: String): File {
        val file = File(presetsDir, fileName(name))
        if (file.isFile) return file
        return File(presetsDir, legacyFileName(name)).takeIf { it.isFile } ?: file
    }

    private fun displayName(uri: Uri): String {
        context.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { if (it.moveToFirst()) return it.getString(0) }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "imported"
    }

    private companion object {
        private const val TAG = "Repository"

        /** Files staged by [stageAsset] before validation; never shown in the asset list. */
        const val STAGED_SUFFIX = ".importing"
    }

}
