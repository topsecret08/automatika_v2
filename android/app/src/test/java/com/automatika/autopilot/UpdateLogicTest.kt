package com.automatika.autopilot

import com.automatika.autopilot.data.UpdateLogic
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream

class UpdateLogicTest {
    private val release = """
        {"tag_name":"v0.9.8","assets":[
          {"name":"automatika-v0.9.8.zip","browser_download_url":"https://x/z.zip","size":10},
          {"name":"Autopilot-v0.9.8.apk","browser_download_url":"https://x/a.apk","size":9000},
          {"name":"SHA256SUMS-apk.txt","browser_download_url":"https://x/s.txt","size":88}
        ]}
    """.trimIndent()

    @Test
    fun parsesApkAndSumsFromRelease() {
        val info = UpdateLogic.parseRelease(release)
        assertEquals("v0.9.8", info.tag)
        assertEquals("Autopilot-v0.9.8.apk", info.apk.name)
        assertEquals(9000L, info.apk.size)
        assertEquals("https://x/s.txt", info.sums.url)
    }

    @Test
    fun rejectsReleaseWithoutApk() {
        val json = """{"tag_name":"v1.0.0","assets":[{"name":"SHA256SUMS-apk.txt","browser_download_url":"https://x/s"}]}"""
        assertThrows(IllegalArgumentException::class.java) { UpdateLogic.parseRelease(json) }
    }

    @Test
    fun rejectsNonHttpsDownloads() {
        val json = """{"tag_name":"v1.0.0","assets":[
          {"name":"Autopilot-v1.0.0.apk","browser_download_url":"http://x/a.apk"},
          {"name":"SHA256SUMS-apk.txt","browser_download_url":"https://x/s"}]}"""
        assertThrows(IllegalArgumentException::class.java) { UpdateLogic.parseRelease(json) }
    }

    @Test
    fun comparesTagWithInstalledVersion() {
        assertTrue(UpdateLogic.isNewer("v0.9.8", "0.9.7"))
        assertTrue(UpdateLogic.isNewer("v1.0.0", "0.9.9"))
        assertFalse(UpdateLogic.isNewer("v0.9.7", "0.9.7"))
        assertFalse(UpdateLogic.isNewer("v0.9.6", "0.9.7"))
        assertFalse(UpdateLogic.isNewer("v0.9.7-beta", "0.9.7"))
    }

    @Test
    fun findsShaForExactFileName() {
        val sums = "ABCDEF  Autopilot-v0.9.8.apk\n1234  other.apk\n"
        assertEquals("abcdef", UpdateLogic.expectedSha256(sums, "Autopilot-v0.9.8.apk"))
        assertNull(UpdateLogic.expectedSha256(sums, "Autopilot-v0.9.9.apk"))
    }

    @Test
    fun sha256MatchesKnownVector() {
        val hex = UpdateLogic.sha256Hex(ByteArrayInputStream("abc".toByteArray()))
        assertEquals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hex)
    }
}
