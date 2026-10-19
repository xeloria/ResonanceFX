// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.data

import io.github.resonancefx.model.Catalog
import org.json.JSONObject

/** An immutable set of effect values. Unknown keys are dropped, missing keys use defaults. */
class Settings private constructor(private val values: Map<String, Any>) {

    operator fun get(key: String): Any = values[key] ?: Catalog.defaults.getValue(key)
    fun bool(key: String) = get(key) as Boolean
    fun int(key: String) = (get(key) as Number).toInt()
    fun string(key: String) = get(key) as String

    fun with(key: String, value: Any): Settings {
        val default = Catalog.defaults[key] ?: return this
        require(default::class == value::class) { "Wrong type for $key: ${value::class.simpleName}" }
        return Settings(values + (key to value))
    }

    /** Keys whose effective value differs between this and [other]. */
    fun diff(other: Settings): Set<String> = Catalog.defaults.keys.filterTo(HashSet()) { this[it] != other[it] }

    fun toJson(): JSONObject = JSONObject().apply {
        put("format", FORMAT)
        val v = JSONObject()
        Catalog.defaults.keys.sorted().forEach { key -> if (this@Settings[key] != Catalog.defaults[key]) v.put(key, this@Settings[key]) }
        put("values", v)
    }

    override fun equals(other: Any?) = other is Settings && diff(other).isEmpty()
    override fun hashCode() = Catalog.defaults.keys.sumOf { this[it].hashCode() }

    companion object {
        const val FORMAT = "resonancefx.settings.v1"
        val DEFAULT = Settings(emptyMap())

        fun fromJson(json: JSONObject): Settings {
            val v = json.optJSONObject("values") ?: json
            val parsed = HashMap<String, Any>()
            Catalog.defaults.forEach { (key, default) ->
                if (!v.has(key)) return@forEach
                val raw = v.get(key)
                val value: Any? = when (default) {
                    is Boolean -> raw as? Boolean ?: raw.toString().toBooleanStrictOrNull()
                    is Int -> (raw as? Number)?.toInt() ?: raw.toString().toIntOrNull()
                    is String -> raw.toString()
                    else -> null
                }
                if (value != null) parsed[key] = clamp(key, value)
            }
            return Settings(parsed)
        }

        /** Keeps imported presets inside each control's range so a bad file can't break the engine. */
        private fun clamp(key: String, value: Any): Any {
            val control = Catalog.controlsByKey[key]?.second ?: return value
            return when (control) {
                is io.github.resonancefx.model.Slider -> (value as Int).coerceIn(control.range)
                is io.github.resonancefx.model.Choice -> (value as Int).coerceIn(control.options.indices)
                else -> value
            }
        }
    }
}
