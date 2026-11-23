// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.driver.Diagnosis
import org.junit.Assert.assertEquals
import org.junit.Test

/** The app-side half of DIAGNOSTICS.md: each observable state maps to exactly one code. */
class DiagnosisTest {
    private fun diag(
        registered: Boolean = false,
        service: Boolean = true,
        attached: Boolean = false,
        configured: Boolean? = null,
        factory: String? = null,
        config: Boolean = true,
        aidl: Boolean = true,
    ) = Diagnosis.of(registered, service, attached, configured, factory, config, aidl)

    @Test fun registeredAndAttachedIsOk() {
        assertEquals(Diagnosis.OK, diag(registered = true, attached = true))
    }

    @Test fun rejectedStreamIsE12() {
        assertEquals(Diagnosis.E12, diag(registered = true, attached = true, configured = false))
    }

    @Test fun registeredButServiceStoppedIsE13() {
        assertEquals(Diagnosis.E13, diag(registered = true, service = false))
    }

    @Test fun noConfigOnTheRomIsE04() {
        assertEquals(Diagnosis.E04, diag(config = false, factory = null))
    }

    @Test fun configWithoutOurEntryIsE06() {
        assertEquals(Diagnosis.E06, diag(config = true, factory = null))
    }

    @Test fun aidlRomWithoutAnAidlEngineIsE07() {
        assertEquals(Diagnosis.E07, diag(factory = "AIDL", aidl = false))
    }

    @Test fun installedButNotRegisteredIsE14() {
        assertEquals(Diagnosis.E14, diag(factory = "HIDL/legacy", aidl = false))
    }
}
