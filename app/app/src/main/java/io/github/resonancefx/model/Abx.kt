// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.model

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/** Statistics and history for the blind listening test (ABX). */
object Abx {
    val TRIAL_COUNTS = listOf(10, 16, 20)

    /**
     * Probability of getting at least [correct] of [trials] right by guessing (one-sided binomial
     * test, p = 1/2 per trial). Below 0.05 the listener very likely heard a real difference.
     */
    fun pValue(correct: Int, trials: Int): Double {
        if (trials <= 0) return 1.0
        var p = 0.0
        var c = 1.0  // C(trials, 0)
        for (k in 0..trials) {
            if (k >= correct) p += c
            c = c * (trials - k) / (k + 1)
        }
        return p / Math.pow(2.0, trials.toDouble())
    }

    /** Fewest correct answers that would be significant (p < 0.05). */
    fun needed(trials: Int): Int = (0..trials).first { pValue(it, trials) < 0.05 }

    data class Result(val time: Long, val target: String, val output: String, val correct: Int, val trials: Int) {
        val p: Double get() = pValue(correct, trials)
        val heard: Boolean get() = p < 0.05
    }

    private const val MAX_HISTORY = 50

    fun history(context: Context): List<Result> {
        val raw = prefs(context).getString("history", null) ?: return emptyList()
        return runCatching {
            val a = JSONArray(raw)
            (0 until a.length()).map { i ->
                val o = a.getJSONObject(i)
                Result(o.getLong("time"), o.getString("target"), o.optString("output"), o.getInt("correct"), o.getInt("trials"))
            }
        }.getOrDefault(emptyList())
    }

    fun record(context: Context, r: Result) {
        val list = (listOf(r) + history(context)).take(MAX_HISTORY)
        val a = JSONArray()
        list.forEach {
            a.put(JSONObject().put("time", it.time).put("target", it.target).put("output", it.output)
                .put("correct", it.correct).put("trials", it.trials))
        }
        prefs(context).edit().putString("history", a.toString()).apply()
    }

    private fun prefs(context: Context) = context.getSharedPreferences("abx", Context.MODE_PRIVATE)
}
