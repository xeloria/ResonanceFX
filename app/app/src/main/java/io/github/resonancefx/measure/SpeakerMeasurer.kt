// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.measure

import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import android.os.Build
import io.github.resonancefx.model.Composite
import io.github.resonancefx.service.EngineService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import kotlin.coroutines.coroutineContext
import kotlin.math.max
import kotlin.math.min

/**
 * Measures the current output with the phone's microphone: plays [SWEEPS] sweeps with effects
 * paused, analyses them and fits a correction. The recording stays in memory and is discarded.
 */
class SpeakerMeasurer(context: Context) {
    private val audio = context.getSystemService(AudioManager::class.java)

    /** Media volume as a fraction of its maximum. */
    val volume: Float
        get() = audio.getStreamVolume(AudioManager.STREAM_MUSIC).toFloat() /
            max(1, audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC))

    /** [progress] receives 0..1. Needs RECORD_AUDIO. */
    suspend fun measure(progress: (Float) -> Unit): Composite.Measurement = withContext(Dispatchers.Default) {
        if (audio.getStreamVolume(AudioManager.STREAM_MUSIC) == 0) {
            throw MeasureException("Media volume is muted. Turn it up to a normal listening level.")
        }
        val sweep = SpeakerAnalysis.sweep()
        // Measure the speaker itself, not the speaker plus our own effects.
        EngineService.setBypass(true)
        try {
            delay(400)
            val captures = (0 until SWEEPS).map { k ->
                val recording = capture(sweep) { p -> progress((k + p) / SWEEPS * 0.9f) }
                SpeakerAnalysis.analyse(recording, sweep)
            }
            progress(0.95f)
            CorrectionFitter.fit(SpeakerAnalysis.combine(captures)).also { progress(1f) }
        } finally {
            EngineService.setBypass(false)
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun capture(sweep: FloatArray, progress: (Float) -> Unit): FloatArray {
        val rate = SpeakerAnalysis.RATE
        // UNPROCESSED is the flattest source where the phone offers it; VOICE_RECOGNITION is
        // required to skip noise suppression and automatic gain everywhere else.
        val source = if (Build.VERSION.SDK_INT >= 24 &&
            audio.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED) == "true"
        ) MediaRecorder.AudioSource.UNPROCESSED else MediaRecorder.AudioSource.VOICE_RECOGNITION
        val minBuffer = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        if (minBuffer <= 0) throw MeasureException("This phone's microphone can't record at $rate Hz.")
        val record = AudioRecord(source, rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, max(minBuffer, rate))
        if (record.state != AudioRecord.STATE_INITIALIZED) {
            record.release()
            throw MeasureException("Couldn't open the microphone. Another app may be using it.")
        }
        val track = sweepTrack(sweep)
        val total = SpeakerAnalysis.recordFrames
        val lead = (SpeakerAnalysis.LEAD_SECONDS * rate).toInt()
        val out = FloatArray(total)
        val chunk = ShortArray(2048)
        try {
            record.startRecording()
            var got = 0
            var playing = false
            while (got < total) {
                coroutineContext.ensureActive()
                val n = record.read(chunk, 0, min(chunk.size, total - got))
                if (n < 0) throw MeasureException("The microphone stopped recording (error $n).")
                for (i in 0 until n) out[got + i] = chunk[i] / 32768f
                got += n
                if (!playing && got >= lead) {
                    track.play()
                    playing = true
                }
                progress(got.toFloat() / total)
            }
        } finally {
            runCatching { record.stop() }
            record.release()
            runCatching { track.stop() }
            track.release()
        }
        return out
    }

    private fun sweepTrack(sweep: FloatArray): AudioTrack {
        val pcm = ShortArray(sweep.size * 2)
        for (i in sweep.indices) {
            val v = (sweep[i] * 32767).toInt().toShort()
            pcm[i * 2] = v
            pcm[i * 2 + 1] = v
        }
        val track = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(SpeakerAnalysis.RATE)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .build(),
            )
            .setTransferMode(AudioTrack.MODE_STATIC)
            .setBufferSizeInBytes(pcm.size * 2)
            .build()
        track.write(pcm, 0, pcm.size)
        return track
    }

    companion object {
        const val SWEEPS = 2
    }
}
