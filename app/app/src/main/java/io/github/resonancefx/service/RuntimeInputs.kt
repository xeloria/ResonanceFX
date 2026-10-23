// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.service

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import androidx.core.content.ContextCompat
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.log10
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Reports the music stream's volume in dBFS x100 (what loudness compensation follows) and whether
 * Android has already applied it to the mix the engine processes ([preScaled]).
 */
class VolumeWatcher(private val context: Context, private val onVolume: (db: Int, preScaled: Boolean) -> Unit) {
    private val audio = context.getSystemService(AudioManager::class.java)
    private var registered = false

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.getIntExtra(EXTRA_STREAM_TYPE, -1) == AudioManager.STREAM_MUSIC) report()
        }
    }

    fun start() {
        if (registered) return
        // Sent by the system only, which still reaches non-exported receivers.
        ContextCompat.registerReceiver(context, receiver, IntentFilter(ACTION_VOLUME_CHANGED), ContextCompat.RECEIVER_NOT_EXPORTED)
        registered = true
        report()
    }

    fun stop() {
        if (!registered) return
        context.unregisterReceiver(receiver)
        registered = false
    }

    /** Also called when the output device changes: each device has its own volume curve. */
    fun report() {
        if (registered) onVolume((currentDb() * 100).roundToInt().coerceIn(-12000, 0), outputType() in softwareVolume)
    }

    private fun currentDb(): Float {
        val stream = AudioManager.STREAM_MUSIC
        val index = audio.getStreamVolume(stream)
        val max = audio.getStreamMaxVolume(stream).coerceAtLeast(1)
        if (index <= 0) return -120f
        if (Build.VERSION.SDK_INT >= 28) {
            runCatching { audio.getStreamVolumeDb(stream, index, outputType()) }.getOrNull()
                ?.takeIf { it.isFinite() && it <= 0f }?.let { return it }
        }
        // Typical Android media curve: about 60 dB across the steps, steeper at the bottom.
        val x = index.toFloat() / max
        return (40f * log10(x)).coerceAtLeast(-60f)
    }

    private fun outputType(): Int {
        val outputs = audio.getDevices(AudioManager.GET_DEVICES_OUTPUTS).map { it.type }
        return listOf(
            AudioDeviceInfo.TYPE_BLUETOOTH_A2DP, AudioDeviceInfo.TYPE_USB_HEADSET, AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
            AudioDeviceInfo.TYPE_WIRED_HEADSET, AudioDeviceInfo.TYPE_USB_DEVICE,
        ).firstOrNull { it in outputs } ?: AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
    }

    companion object {
        /**
         * Outputs where Android scales the mix by the volume before the effects run (measured on a
         * Galaxy S24 Ultra: 16 dB more input level from volume step 4 to 8 on the speaker).
         * Bluetooth devices normally set their volume themselves (absolute volume) and get the
         * full-level mix.
         */
        private val softwareVolume = setOf(
            AudioDeviceInfo.TYPE_BUILTIN_SPEAKER, AudioDeviceInfo.TYPE_WIRED_HEADPHONES, AudioDeviceInfo.TYPE_WIRED_HEADSET,
            AudioDeviceInfo.TYPE_USB_HEADSET, AudioDeviceInfo.TYPE_USB_DEVICE,
        )
        private const val ACTION_VOLUME_CHANGED = "android.media.VOLUME_CHANGED_ACTION"
        private const val EXTRA_STREAM_TYPE = "android.media.EXTRA_VOLUME_STREAM_TYPE"
    }
}

/**
 * Head yaw from a head-tracking headset (Android 13+ dynamic HEAD_TRACKER sensor), in degrees
 * x100, positive when the head turns right. "Front" slowly follows where the listener keeps
 * facing, so the sound stage re-centres after turning in a chair or walking around a corner.
 */
class HeadTracker(context: Context, private val onYaw: (Int) -> Unit) {
    private val sensors = context.getSystemService(SensorManager::class.java)
    private val handler = Handler(Looper.getMainLooper())
    private var sensor: Sensor? = null
    private var wanted = false
    private var reference = Float.NaN
    private var lastEventMs = 0L
    private var lastSentMs = 0L
    private var lastSent = Int.MIN_VALUE

    private val listener = object : SensorEventListener {
        override fun onSensorChanged(event: SensorEvent) = handle(event.values)
        override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) = Unit
    }

    private val dynamicCallback = object : SensorManager.DynamicSensorCallback() {
        override fun onDynamicSensorConnected(sensor: Sensor) = refresh()
        override fun onDynamicSensorDisconnected(sensor: Sensor) = refresh()
    }

    val available: Boolean get() = Build.VERSION.SDK_INT >= 33

    fun setEnabled(enabled: Boolean) {
        if (!available || enabled == wanted) return
        wanted = enabled
        if (enabled) sensors.registerDynamicSensorCallback(dynamicCallback, handler) else sensors.unregisterDynamicSensorCallback(dynamicCallback)
        refresh()
    }

    private fun refresh() {
        if (Build.VERSION.SDK_INT < 33) return
        val next = if (wanted) sensors.getDynamicSensorList(Sensor.TYPE_HEAD_TRACKER).firstOrNull() else null
        if (next == sensor) return
        sensor?.let { sensors.unregisterListener(listener, it) }
        sensor = next
        reference = Float.NaN
        if (next != null) {
            Log.i(TAG, "Head tracking with ${next.name}")
            sensors.registerListener(listener, next, SensorManager.SENSOR_DELAY_GAME, handler)
        } else {
            send(0)
        }
    }

    private fun handle(v: FloatArray) {
        if (v.size < 3) return
        // values[0..2]: rotation vector (axis x angle, radians) of the head in the tracker's
        // frame: X right ear, Y nose, Z up. Yaw is the rotation about Z.
        val angle = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
        val s = if (angle > 1e-6f) sin(angle / 2) / angle else 0.5f
        val w = cos(angle / 2)
        val x = v[0] * s
        val y = v[1] * s
        val z = v[2] * s
        // Counter-clockwise about Z (seen from above) is a turn to the left: negate.
        val yaw = -(atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)) * 180f / PI.toFloat())

        val now = SystemClock.elapsedRealtime()
        val dt = ((now - lastEventMs).coerceIn(0, 1000)) / 1000f
        lastEventMs = now
        if (reference.isNaN()) reference = yaw
        // Re-centre with a 12 s time constant, along the shortest way round.
        reference = wrap(reference + wrap(yaw - reference) * (1 - exp(-dt / RECENTRE_SECONDS)))
        val relative = (wrap(yaw - reference) * 100).roundToInt()
        // About 30 updates a second is plenty; skip changes under half a degree.
        if (now - lastSentMs >= 33 && kotlin.math.abs(relative - lastSent) >= 50) {
            lastSentMs = now
            send(relative)
        }
    }

    private fun send(yaw: Int) {
        lastSent = yaw
        onYaw(yaw)
    }

    private fun wrap(deg: Float): Float {
        var d = deg % 360f
        if (d > 180f) d -= 360f
        if (d < -180f) d += 360f
        return d
    }

    companion object {
        private const val TAG = "HeadTracker"
        private const val RECENTRE_SECONDS = 12f
    }
}
