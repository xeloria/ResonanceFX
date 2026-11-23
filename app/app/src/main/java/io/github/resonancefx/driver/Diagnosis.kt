// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.driver

/**
 * App-side diagnosis using the same code vocabulary as the module (see DIAGNOSTICS.md), limited
 * to what an unprivileged app can observe. The module's Action has the authoritative answer for
 * mount, linker-namespace and factory-log failures, where the app cannot look.
 */
enum class Diagnosis(val code: String, val reason: String) {
    OK("OK", "Engine registered."),
    E04("E04", "No audio effects config was found on this ROM."),
    E06("E06", "No effect config on this ROM contains a ResonanceFX entry (module not installed, not mounted, or not patched)."),
    E07("E07", "This ROM uses the AIDL effect HAL, but the module has no AIDL engine for it."),
    E12("E12", "The engine is registered but not attached to a stream."),
    E13("E13", "The engine service is not running (master switch off or service stopped)."),
    E14("E14", "The module is installed, but the effect HAL has not registered the engine."),
    ;

    companion object {
        fun of(
            registered: Boolean,
            serviceRunning: Boolean,
            attached: Boolean,
            configured: Boolean?,
            factory: String?,
            configFound: Boolean,
            aidlEngine: Boolean,
        ): Diagnosis = when {
            registered && configured == false -> E12
            registered && attached -> OK
            registered -> if (serviceRunning) E12 else E13
            factory == null -> if (configFound) E06 else E04
            factory.startsWith("AIDL") && !aidlEngine -> E07
            else -> E14
        }
    }
}
