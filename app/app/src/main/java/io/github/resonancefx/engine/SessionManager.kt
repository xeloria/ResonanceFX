// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.engine

import android.util.Log
import io.github.resonancefx.data.AttachMode
import io.github.resonancefx.data.Settings
import io.github.resonancefx.driver.EngineEffect
import io.github.resonancefx.driver.Params
import io.github.resonancefx.model.Catalog
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Owns every live engine instance. In [AttachMode.GLOBAL] that's one instance on session 0; in
 * [AttachMode.PER_APP] one per player session. Must be used from a single thread.
 */
class SessionManager(private val applier: Applier) {

    data class Attached(val sessionId: Int, val packageName: String)

    private val effects = LinkedHashMap<Int, Pair<String, EngineEffect>>()
    private var settings: Settings = Settings.DEFAULT
    private var mode = AttachMode.GLOBAL
    private var started = false  // setMode() has run: attaching is wanted
    // Values the service measures (system volume, head yaw), re-sent to every new instance.
    private val runtime = LinkedHashMap<Int, Int>()
    // Effects paused while the app measures a speaker.
    private var bypassed = false

    private val _attached = MutableStateFlow<List<Attached>>(emptyList())
    val attached: StateFlow<List<Attached>> = _attached.asStateFlow()

    private val _error = MutableStateFlow<String?>(null)
    val error: StateFlow<String?> = _error.asStateFlow()

    val primary: EngineEffect? get() = effects.values.firstOrNull()?.second

    fun setMode(newMode: AttachMode) {
        val first = !started
        started = true
        if (!first && newMode == mode && (newMode != AttachMode.GLOBAL || effects.containsKey(GLOBAL_SESSION))) return
        mode = newMode
        releaseAll()
        if (newMode == AttachMode.GLOBAL) attach(GLOBAL_SESSION, "android")
    }

    fun onSessionOpened(sessionId: Int, packageName: String) {
        if (mode != AttachMode.PER_APP || sessionId <= 0) return
        // A player reuses its package for a new session: drop the stale one.
        effects.entries.filter { it.value.first == packageName && it.key != sessionId }.map { it.key }.forEach(::detach)
        attach(sessionId, packageName)
    }

    fun onSessionClosed(sessionId: Int) {
        if (mode == AttachMode.PER_APP) detach(sessionId)
    }

    fun update(newSettings: Settings) {
        val changed = newSettings.diff(settings)
        settings = newSettings
        if (changed.isEmpty()) return
        effects.values.forEach { (_, fx) ->
            runSafely {
                applier.applyChanged(fx, newSettings, changed)
                if (bypassed) fx.enabled = false
            }
        }
        reattachIfLost()
    }

    /** Sends a live measurement to every instance; repeats of the last value are skipped. */
    fun setRuntime(param: Int, value: Int) {
        if (runtime.put(param, value) == value) return
        effects.values.forEach { (_, fx) -> runSafely { fx.set(param, value) } }
    }

    /** Pauses every instance (true) or restores the master switch (false). */
    fun setBypass(on: Boolean) {
        bypassed = on
        effects.values.forEach { (_, fx) -> runSafely { fx.enabled = !on && settings.bool(Catalog.KEY_MASTER) } }
    }

    /**
     * Periodic health check. One `GET_ENABLED` round-trip per live instance: replaces engine
     * handles that died with audioserver, and classifies the tick for the caller's cadence:
     * true = keep the fast cadence (audio can be flowing — some instance is enabled, or
     * nothing is attached and the global retry is trying, or a handle was just replaced);
     * false = every instance answered and is disabled (idle), where periodic commands are the
     * only thing going through and the next probe can wait longer — this is the state where
     * the probe can audibly disturb a suspended/offloaded Bluetooth output thread.
     */
    fun watchdog(): Boolean {
        val probe = reattachIfLost()
        return probe.anyEnabled || effects.isEmpty() || probe.dead.isNotEmpty()
    }

    /** Re-applies everything, e.g. after the output route or sample rate changed. */
    fun reapply() {
        reattachIfLost()
        effects.values.forEach { (_, fx) -> runSafely { applyAll(fx) } }
    }

    private fun applyAll(fx: EngineEffect) {
        applier.applyAll(fx, settings)
        runtime.forEach { (param, value) -> fx.set(param, value) }
        if (bypassed) fx.enabled = false
    }

    fun releaseAll() {
        effects.values.forEach { it.second.close() }
        effects.clear()
        publish()
    }

    private fun attach(sessionId: Int, packageName: String) {
        if (effects.containsKey(sessionId)) return
        try {
            val fx = EngineEffect(sessionId)
            effects[sessionId] = packageName to fx
            try {
                applyAll(fx)
            } catch (e: Throwable) {
                // A half-applied instance must not stay registered: it still answers GET_ENABLED,
                // so reattachIfLost() would never replace it and the session slot (per-app mode)
                // stays occupied by an engine that was never enabled.
                effects.remove(sessionId)
                fx.close()
                throw e
            }
            _error.value = null
            Log.i(TAG, "Attached engine to session $sessionId ($packageName)")
        } catch (e: Throwable) {
            Log.e(TAG, "Can't attach to session $sessionId", e)
            _error.value = when {
                !EngineEffect.isRegistered() -> "The ResonanceFX engine isn't loaded. Install the module and reboot."
                else -> "Couldn't attach to session $sessionId: ${e.message}"
            }
        }
        publish()
    }

    private fun detach(sessionId: Int) {
        effects.remove(sessionId)?.second?.close()
        publish()
    }

    /**
     * Audioserver restarts (crash, module repair, some ROMs on route changes) silently kill every
     * effect handle. Detect that and attach a fresh instance so effects come back by themselves.
     * Also reports what it found: [Probe.anyEnabled] as used by [watchdog], [Probe.dead] as the
     * list of sessions that did lose their handle.
     */
    private class Probe(val dead: List<Pair<Int, String>>, val anyEnabled: Boolean)

    private fun reattachIfLost(): Probe {
        // A dead audioserver makes every getParameter fail; a live engine always answers this.
        val dead = ArrayList<Pair<Int, String>>()
        var anyEnabled = false
        for ((id, value) in effects.entries.toList()) {
            val enabled = value.second.getInt(Params.GET_ENABLED)
            if (enabled == null) dead += id to value.first
            else if (enabled != 0) anyEnabled = true
        }
        dead.forEach { (id, pkg) ->
            Log.i(TAG, "Session $id lost its engine handle, re-attaching")
            detach(id)
            attach(id, pkg)
        }
        // An attach made while audioserver was still restarting fails ("Cannot initialize effect
        // engine"); keep trying instead of staying off until the app restarts.
        if (started && mode == AttachMode.GLOBAL && !effects.containsKey(GLOBAL_SESSION)) attach(GLOBAL_SESSION, "android")
        return Probe(dead, anyEnabled)
    }

    private inline fun runSafely(block: () -> Unit) {
        try { block() } catch (e: Throwable) { Log.w(TAG, "apply failed", e) }
    }

    private fun publish() {
        _attached.value = effects.map { (id, v) -> Attached(id, v.first) }
    }

    companion object {
        private const val TAG = "SessionManager"
        const val GLOBAL_SESSION = 0
    }
}
