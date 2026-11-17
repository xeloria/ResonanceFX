// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.localization

import android.app.Activity
import android.app.LocaleManager as PlatformLocaleManager
import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.LocaleList
import io.github.resonancefx.R
import java.util.Locale

/**
 * In-app language selection.
 *
 * Android picks resources by the device language automatically; this only adds an override.
 * On Android 13+ the platform per-app locale is the source of truth (it also reaches services
 * and notifications). Below that the activity and service contexts are wrapped instead, which
 * covers everything this app draws.
 */
object AppLanguage {
    private const val PREFS = "app"
    private const val KEY = "language"
    private const val PLATFORM_LOCALES = 33

    /** Empty string = follow the system. */
    val SUPPORTED = listOf("en", "ja", "ru", "zh-CN", "es", "ko", "fr")

    fun current(context: Context): String {
        if (Build.VERSION.SDK_INT >= PLATFORM_LOCALES) {
            val locales = context.getSystemService(PlatformLocaleManager::class.java).applicationLocales
            if (!locales.isEmpty) return locales.toLanguageTags()
        }
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY, "").orEmpty()
    }

    /** Persists the choice and applies it; easy to call from a click handler. */
    fun set(context: Context, activity: Activity?, tag: String) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putString(KEY, tag).apply()
        if (Build.VERSION.SDK_INT >= PLATFORM_LOCALES) {
            val manager = context.getSystemService(PlatformLocaleManager::class.java)
            manager.applicationLocales =
                if (tag.isEmpty()) LocaleList.getEmptyLocaleList() else LocaleList.forLanguageTags(tag)
            // The platform recreates the activity when the locales actually change.
        } else {
            activity?.recreate()
        }
    }

    /** Wraps a base context for API < 33; newer platforms do this app-wide themselves. */
    fun wrap(base: Context): Context {
        if (Build.VERSION.SDK_INT >= PLATFORM_LOCALES) return base
        val tag = base.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY, "").orEmpty()
        if (tag.isEmpty()) return base
        val locale = Locale.forLanguageTag(tag)
        val config = Configuration(base.resources.configuration)
        // setLocale exists on every supported API; setLocales (the full list) only from 24.
        @Suppress("DEPRECATION")
        config.setLocale(locale)
        if (Build.VERSION.SDK_INT >= 24) config.setLocales(LocaleList(locale))
        return base.createConfigurationContext(config)
    }

    /** Native name of a language, shown in the picker (never translated). */
    fun label(context: Context, tag: String): String = context.getString(
        when (tag) {
            "" -> R.string.settings_language_system
            "en" -> R.string.language_name_en
            "ja" -> R.string.language_name_ja
            "ru" -> R.string.language_name_ru
            "zh-CN" -> R.string.language_name_zh
            "es" -> R.string.language_name_es
            "ko" -> R.string.language_name_ko
            "fr" -> R.string.language_name_fr
            else -> R.string.language_name_en
        },
    )
}
