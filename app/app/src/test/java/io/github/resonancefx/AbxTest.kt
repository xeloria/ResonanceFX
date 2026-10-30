// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.model.Abx
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AbxTest {
    @Test fun binomialTailMatchesTheTables() {
        // Standard ABX tables: 12/16 is p = 0.038, 11/16 p = 0.105, 9/10 p = 0.011.
        assertEquals(0.0384, Abx.pValue(12, 16), 0.0005)
        assertEquals(0.1051, Abx.pValue(11, 16), 0.0005)
        assertEquals(0.0107, Abx.pValue(9, 10), 0.0005)
        assertEquals(1.0, Abx.pValue(0, 16), 1e-12)
        assertEquals(1.0 / 65536, Abx.pValue(16, 16), 1e-12)
    }

    @Test fun neededCorrectAnswers() {
        assertEquals(9, Abx.needed(10))
        assertEquals(12, Abx.needed(16))
        assertEquals(15, Abx.needed(20))
    }

    @Test fun verdictFollowsTheFivePercentLevel() {
        assertTrue(Abx.Result(0, "x", "", 12, 16).heard)
        assertFalse(Abx.Result(0, "x", "", 11, 16).heard)
    }
}
