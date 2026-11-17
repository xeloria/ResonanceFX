// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.service.HeadOrientation
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

class HeadOrientationTest {
    private fun qz(deg: Float): FloatArray {
        val h = (deg * PI / 180.0 / 2.0).toFloat()
        return floatArrayOf(cos(h), 0f, 0f, sin(h))
    }

    private fun qx(deg: Float): FloatArray {
        val h = (deg * PI / 180.0 / 2.0).toFloat()
        return floatArrayOf(cos(h), sin(h), 0f, 0f)
    }

    private fun assertNear(expected: Float, actual: Float, tolerance: Float = 1e-3f) =
        assertEquals(expected, actual, tolerance)

    @Test fun identityHasNoYaw() {
        assertNear(0f, HeadOrientation.yawDegrees(HeadOrientation.IDENTITY))
        assertNear(0f, HeadOrientation.angleDegrees(HeadOrientation.IDENTITY, HeadOrientation.IDENTITY))
    }

    @Test fun yawIsPositiveToTheRight() {
        // A counter-clockwise rotation about Z is a turn to the left, so the yaw is negative.
        assertNear(-90f, HeadOrientation.yawDegrees(qz(90f)))
        assertNear(90f, HeadOrientation.yawDegrees(qz(-90f)))
    }

    @Test fun rotationVectorBecomesTheSameYaw() {
        val q = HeadOrientation.fromRotationVector(floatArrayOf(0f, 0f, (PI / 2).toFloat()))
        assertNear(-90f, HeadOrientation.yawDegrees(q))
    }

    @Test fun recentringAtTheCurrentHeadingIsIdentity() {
        val q = qz(90f)
        val reference = HeadOrientation.yawDegrees(q)
        val relative = HeadOrientation.recentre(q, reference)
        assertNear(0f, HeadOrientation.angleDegrees(relative, HeadOrientation.IDENTITY))
        assertNear(0f, HeadOrientation.yawDegrees(relative))
    }

    @Test fun recentringLeavesPitchAlone() {
        val q = qx(30f)
        val relative = HeadOrientation.recentre(q, 0f)
        assertNear(0f, HeadOrientation.yawDegrees(relative))
        // Still pitched 30 degrees: the orientation is unchanged.
        assertNear(0f, HeadOrientation.angleDegrees(relative, q))
    }

    @Test fun angleMeasuresOrientationDifference() {
        assertNear(10f, HeadOrientation.angleDegrees(HeadOrientation.IDENTITY, qz(10f)))
        assertTrue(HeadOrientation.angleDegrees(qz(10f), qz(-10f)) > 19.9f)
    }
}
