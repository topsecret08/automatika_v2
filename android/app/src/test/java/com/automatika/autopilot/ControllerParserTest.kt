package com.automatika.autopilot

import com.automatika.autopilot.data.ControllerParser
import com.automatika.autopilot.data.HttpControllerApi
import com.automatika.autopilot.ui.processTypeName
import com.automatika.autopilot.ui.sensorLine
import com.automatika.autopilot.ui.watchdogLine
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerParserTest {

    @Test
    fun parsesProcessHistoryAndNamesTheType() {
        val json = JSONArray(
            "[{\"id\":3,\"type\":2,\"started_ms\":1000,\"ended_ms\":4000,\"duration_sec\":3.0," +
                "\"heads_ml\":120,\"body_ml\":860,\"tails_ml\":40,\"rating\":4,\"recipe_id\":\"r1\"}]"
        )
        val rows = ControllerParser.processes(json)
        assertEquals(1, rows.size)
        assertEquals(3L, rows[0].id)
        assertEquals(860.0, rows[0].bodyMl, 1e-9)
        assertEquals("Ректификация", processTypeName(rows[0].type))
        assertEquals("Тип 42", processTypeName(42))
    }

    @Test
    fun parsesAiStatus() {
        val json = JSONObject(
            "{\"db_ready\":true,\"anomaly\":{\"error\":0.5,\"threshold\":2.0,\"is_anomaly\":false," +
                "\"since_ms\":0,\"severity\":0,\"samples\":120},\"action_accuracy\":0.8,\"cache_hit_rate\":0.25}"
        )
        val s = ControllerParser.aiStatus(json)
        assertTrue(s.dbReady)
        assertEquals(120L, s.anomalySamples)
        assertEquals(0.8, s.actionAccuracy, 1e-9)
        assertFalse(s.isAnomaly)
    }

    @Test
    fun calibStateBecomesReadableLines() {
        val json = JSONObject(
            "{\"sensors\":{\"state\":0,\"saved\":false,\"reference\":85.5,\"error\":\"\",\"offsets\":[0.1,0.2]}," +
                "\"scale\":{\"state\":3,\"ready\":true,\"stable\":false,\"grams\":1250.0,\"total_ml\":860.0}," +
                "\"valves\":[{\"idx\":0,\"active\":true,\"ml_per_hour\":120.0}]}"
        )
        val lines = ControllerParser.calibLines(json)
        assertTrue(lines.any { it.startsWith("Датчики: состояние 0, эталон 85.5") })
        assertTrue(lines.any { it == "Весы: готовы, нестабильно, 1250.0 г, всего 860 мл" })
        assertTrue(lines.any { it == "Клапан 1: калибровка идёт, 120 мл/ч" })
    }

    @Test
    fun parsesRecipeListAndDetail() {
        val list = ControllerParser.recipes(
            JSONArray("[{\"id\":\"beer_pils\",\"name\":\"Пильзнер\",\"category\":\"beer\",\"description\":\"Светлое\"},{\"id\":\"\",\"name\":\"x\"}]")
        )
        assertEquals(1, list.size)
        assertEquals("beer", list[0].category)

        val detail = ControllerParser.recipe(
            JSONObject("{\"id\":\"beer_pils\",\"name\":\"Пильзнер\",\"category\":\"beer\",\"description\":\"Светлое\"," +
                "\"ingredients\":\"Солод 5 кг\",\"steps_md\":\"1. Затор\"}")
        )
        assertEquals("Солод 5 кг", detail.ingredients)
        assertEquals("1. Затор", detail.stepsMd)
    }

    @Test
    fun parsesHelpSensorsAndWatchdog() {
        val help = ControllerParser.help(JSONObject("{\"id\":\"01_intro\",\"title\":\"Основы\",\"content_md\":\"# Текст\"}"))
        assertEquals("# Текст", help.contentMd)

        val sensors = ControllerParser.sensors(
            JSONArray("[{\"idx\":0,\"address\":\"28aa\",\"temp\":78.5,\"noise_rms\":0.02,\"age_ms\":150}]")
        )
        assertEquals("Датчик 1: 78.50 °C, шум 0.020 °C, показание 150 мс назад, адрес 28aa", sensorLine(sensors[0]))

        val wd = ControllerParser.watchdog(
            JSONArray("[{\"name\":\"ctrl\",\"timeout_ms\":5000,\"age_ms\":9000,\"max_stale_ms\":5000,\"restarts\":2,\"enabled\":true,\"critical\":true}]")
        )
        assertTrue(watchdogLine(wd[0]).startsWith("ctrl: ЗАВИСЛА"))
    }

    @Test
    fun aiStatusWithoutValuesIsNaN() {
        val s = ControllerParser.aiStatus(JSONObject("{}"))
        assertTrue(s.actionAccuracy.isNaN())
        assertFalse(s.dbReady)
    }

    @Test
    fun parsesMenuTogglesAsDeviceSendsThem() {
        val json = JSONArray(
            "[{\"key\":\"auto92\",\"title\":\"Авто-92+\",\"value\":1,\"role\":2,\"editable\":true}," +
                "{\"key\":\"wifi\",\"title\":\"Wi-Fi\",\"value\":0,\"role\":2,\"editable\":false}]"
        )
        val toggles = ControllerParser.toggles(json)
        assertEquals(2, toggles.size)
        assertEquals("auto92", toggles[0].key)
        assertTrue(toggles[0].value)
        assertTrue(toggles[0].editable)
        assertFalse(toggles[1].editable)
    }

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
        val health = ControllerParser.health(JSONObject("""{"version":"V1.0.1","uptime_s":42}"""))
        assertEquals("V1.0.1", health.version)
        assertEquals(42L, health.uptimeSec)
    }

    @Test
    fun normalizeAddsSchemeAndDropsSlash() {
        assertEquals("http://automatika.local", HttpControllerApi.normalize("automatika.local/"))
        assertEquals("http://192.168.1.50", HttpControllerApi.normalize(" 192.168.1.50 "))
        assertEquals("https://x.local", HttpControllerApi.normalize("https://x.local"))
    }
}
