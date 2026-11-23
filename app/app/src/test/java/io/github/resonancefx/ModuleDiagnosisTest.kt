// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx

import io.github.resonancefx.driver.ModuleDiagnosis
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

/** The module's published verdict is the app's only root-level source; stale copies must not win. */
class ModuleDiagnosisTest {
    @get:Rule val tmp = TemporaryFolder()

    private fun write(header: String, body: String = "E10|Namespace rejects the library|detail|fix") {
        File(tmp.root, "module-diagnosis.txt").writeText("$header\n$body\n")
    }

    @Test fun readsTheModuleVerdict() {
        write("# rfx-diagnosis f=1 v=20700 t=100000 boot=boot-a")
        val m = ModuleDiagnosis.read(tmp.root, now = 200_000, uptime = 0, currentBoot = "boot-a")!!
        assertEquals("E10", m.code)
        assertEquals("Namespace rejects the library", m.short)
        assertEquals("detail", m.detail)
        assertEquals("fix", m.fix)
        assertEquals(20700, m.moduleVersion)
    }

    @Test fun ignoresAFileFromAnotherBoot() {
        write("# rfx-diagnosis f=1 v=20700 t=100000 boot=boot-a")
        assertNull(ModuleDiagnosis.read(tmp.root, now = 200_000, uptime = 0, currentBoot = "boot-b"))
    }

    @Test fun ignoresAnUnknownFormat() {
        write("# rfx-diagnosis f=2 v=20700 t=100000 boot=boot-a")
        assertNull(ModuleDiagnosis.read(tmp.root, now = 200_000, uptime = 0, currentBoot = "boot-a"))
    }

    @Test fun ignoresAStaleFileWhenTheBootIdIsUnreadable() {
        write("# rfx-diagnosis f=1 v=20700 t=10 boot=a")
        // Booted at 200 s; the file was written at 10 s, long before this boot.
        assertNull(ModuleDiagnosis.read(tmp.root, now = 200_000, uptime = 0, currentBoot = null))
    }
}
