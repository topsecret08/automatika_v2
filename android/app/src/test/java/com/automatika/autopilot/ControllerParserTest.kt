package com.automatika.autopilot

import com.automatika.autopilot.data.ControllerParser
import com.automatika.autopilot.data.HttpControllerApi
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerParserTest {

    @Test
    fun parsesEventsArrayAsDeviceSendsIt() {
        val json = JSONArray("[{\"ts_ms\":1,\"type\":2,\"name\":\"WARN\",\"msg\":\"sensor\"}]")
        assertEquals(listOf("WARN  sensor"), ControllerParser.events(json))
        assertEquals(emptyList<String>(), ControllerParser.events(JSONArray("[]")))
    }

    @Test
    fun parsesDeviceStateJson() {
        val json = JSONObject(
            """{"cube":78.5,"col":55.2,"power":42,"running":1,"safety":1,"estop":0,
               "heads":120,"body":640,"tails":0,"mode":"distill","elapsed":600}"""
        )
        val state = ControllerParser.state(json)
        assertEquals(78.5, state.cube, 0.001)
        assertEquals(55.2, state.column, 0.001)
        assertTrue(state.running)
        assertTrue(state.safetyOk)
        assertFalse(state.estop)
        assertEquals(600, state.elapsedSec)
    }

    @Test
    fun missingFieldsUseSafeDefaults() {
        val state = ControllerParser.state(JSONObject("{}"))
        assertTrue(state.cube.isNaN())
        assertFalse(state.running)
        assertTrue(state.safetyOk)
    }

    @Test
    fun healthParsesVersionAndUptime() {
        val health = ControllerParser.health(JSONObject("""{"version":"V0.9.6","uptime_s":42}"""))
        assertEquals("V0.9.6", health.version)
        assertEquals(42L, health.uptimeSec)
    }

    @Test
    fun normalizeAddsSchemeAndDropsSlash() {
        assertEquals("http://automatika.local", HttpControllerApi.normalize("automatika.local/"))
        assertEquals("http://192.168.1.50", HttpControllerApi.normalize(" 192.168.1.50 "))
        assertEquals("https://x.local", HttpControllerApi.normalize("https://x.local"))
    }
}
