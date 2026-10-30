// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.service

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ServiceInfo
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.media.audiofx.AudioEffect
import android.os.Build
import android.os.Handler
import android.os.Looper
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import io.github.resonancefx.App
import io.github.resonancefx.MainActivity
import io.github.resonancefx.R
import io.github.resonancefx.autoeq.AutoEqDatabase
import io.github.resonancefx.data.Route
import io.github.resonancefx.driver.DriverStatus
import io.github.resonancefx.driver.Params
import io.github.resonancefx.engine.Applier
import io.github.resonancefx.engine.SessionManager
import io.github.resonancefx.engine.TestTone
import io.github.resonancefx.model.Catalog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext

/**
 * Keeps engine instances alive (an AudioEffect dies with its process), follows the output route
 * to switch profiles, and recovers automatically when audioserver restarts.
 */
class EngineService : LifecycleService() {

    @OptIn(ExperimentalCoroutinesApi::class)
    private val engineThread = Dispatchers.Default.limitedParallelism(1)
    private val repository get() = App.instance.repository
    private lateinit var applier: Applier
    private lateinit var sessions: SessionManager
    private lateinit var audio: AudioManager
    private var lastSampleRate = 0
    private lateinit var volume: VolumeWatcher
    private lateinit var headTracker: HeadTracker
    private lateinit var orientation: OrientationWatcher

    // Manifest receivers don't get implicit broadcasts on Android 8+; while we run, listen here.
    private val sessionReceiver = SessionReceiver()

    private val deviceCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(added: Array<out AudioDeviceInfo>?) {
            refreshRoute()
            if (::volume.isInitialized) volume.report()
        }
        override fun onAudioDevicesRemoved(removed: Array<out AudioDeviceInfo>?) {
            refreshRoute()
            if (::volume.isInitialized) volume.report()
        }
    }

    override fun onCreate() {
        super.onCreate()
        audio = getSystemService(AudioManager::class.java)
        applier = Applier(
            resolveAsset = { kind, name -> repository.assetFile(kind, name) },
            autoEq = { runCatching { AutoEqDatabase.get(this) }.getOrNull() },
        )
        sessions = SessionManager(applier)
        running.value = true
        engine = sessions
        engineApplier = applier
        bypassHook = { on -> lifecycleScope.launch(engineThread) { sessions.setBypass(on) } }
        instance = this
        startInForeground()

        audio.registerAudioDeviceCallback(deviceCallback, Handler(Looper.getMainLooper()))
        ContextCompat.registerReceiver(
            this, sessionReceiver,
            IntentFilter().apply {
                addAction(AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION)
                addAction(AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION)
            },
            ContextCompat.RECEIVER_EXPORTED,
        )
        refreshRoute()

        volume = VolumeWatcher(this) { db, preScaled ->
            lifecycleScope.launch(engineThread) {
                sessions.setRuntime(Params.LOUDNESS_VOLUME, db)
                sessions.setRuntime(Params.INPUT_LEVEL_OFFSET, if (preScaled) db.coerceAtLeast(-9600) else 0)
            }
        }
        headTracker = HeadTracker(this) { yaw -> lifecycleScope.launch(engineThread) { sessions.setRuntime(Params.SPATIAL_YAW, yaw) } }
        orientation = OrientationWatcher(this) { sideways ->
            lifecycleScope.launch(engineThread) { sessions.setRuntime(Params.CROSSTALK_ORIENTATION, if (sideways) 100 else 0) }
        }
        // Only listen while the module that uses the measurement is on.
        lifecycleScope.launch {
            repository.settings.collect { s ->
                // Loudness compensation follows the volume; the level-dependent modules need it to
                // judge the programme rather than the volume knob.
                val levelModules = listOf("agc.enable", "fet.enable", "mbc.enable", "deq.enable", "deess.enable")
                if ((s.bool("loud.enable") && s.int("loud.mode") == 0) || levelModules.any { s.bool(it) }) volume.start()
                else volume.stop()
                headTracker.setEnabled(s.bool("spatial.enable") && s.bool("spatial.tracking"))
                updateOrientation(s, repository.route.value)
            }
        }

        lifecycleScope.launch(engineThread) {
            // Load the profile first so the very first attach already applies the real settings.
            sessions.update(repository.settings.value)
            sessions.setMode(repository.attachMode)
        }
        lifecycleScope.launch {
            repository.settings.collect { settings ->
                withContext(engineThread) { sessions.update(settings) }
            }
        }
        lifecycleScope.launch {
            combine(repository.route, sessions.attached, sessions.error) { r, a, e -> Triple(r, a, e) }
                .distinctUntilChanged()
                .collect { updateNotification() }
        }
        // Watchdog: re-attach after audioserver restarts and re-upload the impulse response when
        // the output sample rate changes (e.g. switching to a 96 kHz USB DAC).
        lifecycleScope.launch(engineThread) {
            while (isActive) {
                delay(3000)
                val rate = sessions.primary?.getInt(Params.GET_SAMPLING_RATE) ?: 0
                if (rate > 0 && lastSampleRate != 0 && rate != lastSampleRate) sessions.reapply()
                if (rate > 0) lastSampleRate = rate
                sessions.watchdog()
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)
        when (intent?.action) {
            ACTION_SESSION_OPEN -> {
                val id = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, -1)
                val pkg = intent.getStringExtra(AudioEffect.EXTRA_PACKAGE_NAME).orEmpty()
                lifecycleScope.launch(engineThread) { sessions.onSessionOpened(id, pkg) }
            }
            ACTION_SESSION_CLOSE -> {
                val id = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, -1)
                lifecycleScope.launch(engineThread) { sessions.onSessionClosed(id) }
            }
            ACTION_MODE_CHANGED -> lifecycleScope.launch(engineThread) { sessions.setMode(repository.attachMode) }
            ACTION_REAPPLY -> lifecycleScope.launch(engineThread) { sessions.reapply() }
            ACTION_TEST_TONE -> {
                TestTone.play()
                // Log the engine's own verdict mid-tone: makes "is it working?" answerable from
                // logcat alone (adb logcat -s ResonanceFX.Status).
                lifecycleScope.launch(Dispatchers.IO) {
                    delay(2500)
                    android.util.Log.i("ResonanceFX.Status", DriverStatus.query().report())
                }
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        audio.unregisterAudioDeviceCallback(deviceCallback)
        volume.stop()
        headTracker.setEnabled(false)
        orientation.setEnabled(false)
        unregisterReceiver(sessionReceiver)
        // Release synchronously on the engine thread: a leaked AudioEffect keeps processing
        // after we're gone.
        runBlocking(engineThread) { sessions.releaseAll() }
        running.value = false
        engine = null
        bypassHook = null
        instance = null
        super.onDestroy()
    }

    /** Speaker widening follows how the device is held, but only on its own speakers. */
    private fun updateOrientation(s: io.github.resonancefx.data.Settings, route: Route) {
        val follow = s.bool("xtc.enable") && s.bool(Catalog.KEY_XTC_SIDEWAYS) && route == Route.SPEAKER
        orientation.setEnabled(follow)
        if (!follow) lifecycleScope.launch(engineThread) { sessions.setRuntime(Params.CROSSTALK_ORIENTATION, 100) }
    }

    private fun refreshRoute() {
        val route = Route.current(audio)
        val changed = route != repository.route.value
        repository.onRouteChanged(route)
        if (changed) lifecycleScope.launch(engineThread) { sessions.reapply() }
        if (::orientation.isInitialized) updateOrientation(repository.settings.value, route)
    }

    private fun startInForeground() {
        val type = if (Build.VERSION.SDK_INT >= 34) ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE else 0
        ServiceCompat.startForeground(this, NOTIFICATION_ID, buildNotification(), type)
    }

    private fun updateNotification() {
        getSystemService(android.app.NotificationManager::class.java).notify(NOTIFICATION_ID, buildNotification())
    }

    private fun buildNotification(): Notification {
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE,
        )
        val error = if (::sessions.isInitialized) sessions.error.value else null
        val attached = if (::sessions.isInitialized) sessions.attached.value.size else 0
        val text = error ?: getString(R.string.notification_text, repository.activeProfileName, attached)
        return NotificationCompat.Builder(this, App.CHANNEL_SERVICE)
            .setSmallIcon(R.drawable.ic_stat_engine)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(text)
            .setContentIntent(open)
            .setOngoing(true)
            .setSilent(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_IMMEDIATE)
            .build()
    }

    companion object {
        private const val NOTIFICATION_ID = 1
        const val ACTION_SESSION_OPEN = "io.github.resonancefx.SESSION_OPEN"
        const val ACTION_SESSION_CLOSE = "io.github.resonancefx.SESSION_CLOSE"
        const val ACTION_MODE_CHANGED = "io.github.resonancefx.MODE_CHANGED"
        const val ACTION_REAPPLY = "io.github.resonancefx.REAPPLY"
        const val ACTION_TEST_TONE = "io.github.resonancefx.TEST_TONE"

        private val running = MutableStateFlow(false)
        val isRunning: StateFlow<Boolean> = running.asStateFlow()

        /** The live session manager while the service runs (for the status screen). */
        @Volatile var engine: SessionManager? = null
            private set
        @Volatile var engineApplier: Applier? = null
            private set
        @Volatile private var bypassHook: ((Boolean) -> Unit)? = null
        @Volatile private var instance: EngineService? = null

        /** Runs [block] on the engine thread; null when the service isn't running. */
        suspend fun <T> onEngine(block: (SessionManager) -> T): T? {
            val service = instance ?: return null
            return withContext(service.engineThread) { block(service.sessions) }
        }

        /** Pauses effects (for a speaker measurement) or restores them. */
        fun setBypass(on: Boolean) {
            bypassHook?.invoke(on)
        }

        fun start(context: Context, action: String? = null, extras: Intent? = null) {
            val intent = Intent(context, EngineService::class.java).setAction(action)
            extras?.extras?.let { intent.putExtras(it) }
            ContextCompat.startForegroundService(context, intent)
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, EngineService::class.java))
        }

        /** Starts the service if the master switch is on, stops it otherwise. */
        fun sync(context: Context) {
            if (App.instance.repository.settings.value.bool(Catalog.KEY_MASTER)) start(context) else stop(context)
        }
    }
}
