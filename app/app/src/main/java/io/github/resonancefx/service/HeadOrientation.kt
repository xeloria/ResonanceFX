// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.service

import kotlin.math.abs
import kotlin.math.acos
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Head-orientation maths, kept free of Android so it can be unit-tested. A quaternion is
 * (w, x, y, z) and maps the head frame to the world frame: x right ear, y nose, z up.
 *
 * Yaw stays in the app's existing convention (degrees, positive when the head turns right),
 * while pitch and roll travel as part of the quaternion: they are gravity-referenced and must
 * not be recentred, or a deliberate tilt would slowly cancel itself.
 */
object HeadOrientation {
    val IDENTITY = floatArrayOf(1f, 0f, 0f, 0f)

    /** TYPE_HEAD_TRACKER values[0..2]: rotation vector (axis x angle, radians) of the head. */
    fun fromRotationVector(v: FloatArray): FloatArray {
        val angle = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
        val s = if (angle > 1e-6f) sin(angle / 2f) / angle else 0.5f
        return normalize(floatArrayOf(cos(angle / 2f), v[0] * s, v[1] * s, v[2] * s))
    }

    /** Heading about the world's vertical axis, positive when the head turns right. */
    fun yawDegrees(q: FloatArray): Float {
        val (w, x, y, z) = q
        val yaw = atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))
        return (-yaw * 180.0 / Math.PI).toFloat()
    }

    /**
     * Removes the slowly-following heading reference from an orientation, leaving pitch and
     * roll untouched: q * Rz(-reference heading). The scene's front ends up where the listener
     * has been facing, while tilts stay referenced to gravity.
     */
    fun recentre(q: FloatArray, referenceYawDeg: Float): FloatArray {
        val h = referenceYawDeg * Math.PI.toFloat() / 180f / 2f
        val c = cos(h)
        val s = sin(h)
        val (w, x, y, z) = q
        return normalize(
            floatArrayOf(
                c * w - s * z,
                c * x - s * y,
                c * y + s * x,
                c * z + s * w,
            ),
        )
    }

    /** Angle between two orientations, in degrees (0..180). */
    fun angleDegrees(a: FloatArray, b: FloatArray): Float {
        val dot = abs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]).coerceAtMost(1f)
        return (2.0 * acos(dot) * 180.0 / Math.PI).toFloat()
    }

    fun normalize(q: FloatArray): FloatArray {
        val n = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
        return if (n > 1e-6f) floatArrayOf(q[0] / n, q[1] / n, q[2] / n, q[3] / n) else IDENTITY
    }
}
