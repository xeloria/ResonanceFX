// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

import android.os.Build
import android.util.Log
import android.util.Property
import java.lang.reflect.Method

/**
 * Exempts this process from hidden-API enforcement (Android 9+): android.util.Property reads
 * Class.getDeclaredMethods of dalvik.system.VMRuntime without going through the restricted
 * reflection path, then VMRuntime.setHiddenApiExemptions(["L"]) whitelists every class. Only used
 * as a fallback when the AudioEffect parameter methods can't be resolved normally.
 */
object HiddenApi {
    @Volatile private var done = false

    @Synchronized
    fun unseal() {
        if (done || Build.VERSION.SDK_INT < 28) return
        done = true
        runCatching {
            @Suppress("UNCHECKED_CAST")
            val methods = Property.of(Class::class.java, Array<Method>::class.java, "DeclaredMethods")
                .get(Class.forName("dalvik.system.VMRuntime")) as Array<Method>
            val getRuntime = methods.first { it.name == "getRuntime" }
            val setExemptions = methods.first { it.name == "setHiddenApiExemptions" }
            setExemptions.invoke(getRuntime.invoke(null), arrayOf("L"))
            Log.i("HiddenApi", "hidden API exemption applied")
        }.onFailure { Log.w("HiddenApi", "could not lift hidden API restrictions", it) }
    }
}
