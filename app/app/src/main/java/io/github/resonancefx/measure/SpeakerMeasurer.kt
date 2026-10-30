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
import io.github.resonancefx.service.VolumeWatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import kotlin.coroutines.coroutineContext
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

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

    /**
     * [progress] receives 0..1. Needs RECORD_AUDIO. With [distortion], louder sweeps follow to
     * find where the speaker starts to distort (see [measureDistortion]).
     */
    suspend fun measure(progress: (Float) -> Unit, distortion: Boolean = false): Composite.Measurement =
        withContext(Dispatchers.Default) {
            if (audio.getStreamVolume(AudioManager.STREAM_MUSIC) == 0) {
                throw MeasureException("Media volume is muted. Turn it up to a normal listening level.")
            }
            val sweep = SpeakerAnalysis.sweep()
            val steps = SWEEPS + if (distortion) DISTORTION_LEVELS.size else 0
            // Measure the speaker itself, not the speaker plus our own effects.
            EngineService.setBypass(true)
            try {
                delay(400)
                val captures = (0 until SWEEPS).map { k ->
                    val recording = capture(sweep) { p -> progress((k + p) / steps * 0.95f) }
                    SpeakerAnalysis.analyse(recording, sweep)
                }
                val fitted = CorrectionFitter.fit(SpeakerAnalysis.combine(captures))
                val d = if (distortion) measureDistortion { k, p -> progress((SWEEPS + k + p) / steps * 0.95f) } else null
                progress(1f)
                fitted.copy(distortion = d)
            } finally {
                EngineService.setBypass(false)
            }
        }

    /**
     * Plays sweeps at rising levels ([DISTORTION_LEVELS]) and records, per protection band, the
     * loudest level that stayed below 5 % THD. Stops as soon as any band passes 20 % or the
     * microphone overloads, so the speaker is never pushed further than needed to find its limit.
     */
    private suspend fun measureDistortion(progress: (Int, Float) -> Unit): Composite.Distortion {
        val bands = SpeakerAnalysis.DISTORTION_BANDS.size
        val clean = arrayOfNulls<Float>(bands)      // loudest clean level so far
        val failed = BooleanArray(bands)
        var lastTested = DISTORTION_LEVELS.first().toFloat()
        var completed = true
        for ((k, level) in DISTORTION_LEVELS.withIndex()) {
            val sweep = SpeakerAnalysis.sweep(10.0.pow(level / 20.0))
            val thd: Array<Double?> = try {
                SpeakerAnalysis.analyseDistortion(capture(sweep) { p -> progress(k, p) }, sweep)
            } catch (e: OverloadException) {
                // Too loud for the microphone (or the speaker): every band ends here.
                completed = false
                break
            }
            lastTested = level.toFloat()
            for (b in 0 until bands) {
                if (failed[b]) continue
                if ((thd[b] ?: 0.0) <= CLEAN_THD) clean[b] = level.toFloat() else failed[b] = true
            }
            if (thd.any { (it ?: 0.0) > STOP_THD }) {
                completed = false
                break
            }
        }
        val first = DISTORTION_LEVELS.first().toFloat()
        val result = List(bands) { b ->
            when {
                // Already distorting at the quietest level: allow 3 dB less than that.
                clean[b] == null -> first - 3f
                failed[b] -> clean[b]
                // Clean as far as we went; if the test stopped early, only that far is known.
                completed -> null
                else -> lastTested
            }
        }
        return Composite.Distortion(VolumeWatcher.levelOffsetDb(audio), result)
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
        /** Sweep levels of the distortion test, dBFS. */
        val DISTORTION_LEVELS = listOf(-12.0, -9.0, -6.0, -3.0, 0.0)
        private const val CLEAN_THD = 0.05
        private const val STOP_THD = 0.2
    }
}
