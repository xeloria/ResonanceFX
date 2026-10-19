// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.engine

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlin.math.PI
import kotlin.math.min
import kotlin.math.sin

/**
 * A short stereo test signal (a soft chord with a slow left/right sweep) played as media, so the
 * user can confirm on the Status screen that the engine sees and processes audio.
 */
object TestTone {
    private const val RATE = 48000
    @Volatile private var track: AudioTrack? = null

    fun play(seconds: Double = 3.0) {
        stop()
        val frames = (RATE * seconds).toInt()
        val pcm = ShortArray(frames * 2)
        val freqs = doubleArrayOf(220.0, 277.18, 329.63, 440.0)
        for (i in 0 until frames) {
            val t = i.toDouble() / RATE
            var s = 0.0
            for (f in freqs) s += sin(2 * PI * f * t)
            s /= freqs.size
            // 50 ms fades avoid clicks; the pan sweep exercises both channels.
            val fade = min(1.0, min(t, seconds - t) / 0.05)
            val pan = 0.5 + 0.5 * sin(2 * PI * t / seconds)
            val v = s * 0.25 * fade * 32767
            pcm[i * 2] = (v * (1 - pan)).toInt().toShort()
            pcm[i * 2 + 1] = (v * pan).toInt().toShort()
        }
        val t = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(RATE)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .build(),
            )
            .setTransferMode(AudioTrack.MODE_STATIC)
            .setBufferSizeInBytes(pcm.size * 2)
            .build()
        t.write(pcm, 0, pcm.size)
        t.notificationMarkerPosition = frames
        t.setPlaybackPositionUpdateListener(object : AudioTrack.OnPlaybackPositionUpdateListener {
            override fun onMarkerReached(track: AudioTrack) = stop()
            override fun onPeriodicNotification(track: AudioTrack) {}
        })
        t.play()
        track = t
    }

    fun stop() {
        track?.let { runCatching { it.stop() }; it.release() }
        track = null
    }
}
