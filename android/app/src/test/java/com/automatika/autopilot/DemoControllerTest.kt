package com.automatika.autopilot

import com.automatika.autopilot.data.ControllerException
import com.automatika.autopilot.data.DemoController
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class DemoControllerTest {

    private var now = 0L
    private val demo = DemoController { now }

    @Test
    fun startRunsAndHeatsCube() = runBlocking {
        demo.start()
        now += 600_000L
        val state = demo.state()
        assertTrue(state.running)
        assertTrue(state.cube > DemoController.AMBIENT_C)
        assertEquals(600, state.elapsedSec)
    }

    @Test
    fun stopKeepsElapsedTime() = runBlocking {
        demo.start()
        now += 60_000L
        demo.stop()
        now += 60_000L
        val state = demo.state()
        assertFalse(state.running)
        assertEquals(60, state.elapsedSec)
    }

    @Test
    fun cubeNeverExceedsMaximum() = runBlocking {
        demo.start()
        now += 10_000_000L
        assertTrue(demo.state().cube <= DemoController.CUBE_MAX_C)
    }

    @Test
    fun alarmBlocksStartUntilReset() = runBlocking {
        demo.triggerAlarm()
        try {
            demo.start()
            fail("запуск должен быть заблокирован аварией")
        } catch (e: ControllerException) {
            // ожидаемо
        }
        demo.resetSafety()
        demo.start()
        assertTrue(demo.state().running)
    }

    @Test
    fun cannotResetWhileRunning() = runBlocking {
        demo.start()
        try {
            demo.resetSafety()
            fail("сброс аварии во время процесса должен быть запрещён")
        } catch (e: ControllerException) {
            // ожидаемо
        }
    }

    @Test
    fun startWithModeRecordsMode() = runBlocking {
        demo.start("beer")
        assertEquals("beer", demo.state().mode)
    }

    @Test
    fun unknownModeIsRejected() = runBlocking {
        try {
            demo.start("unknown")
            fail("неизвестный режим должен отклоняться")
        } catch (e: ControllerException) {
            // ожидаемо
        }
        assertFalse(demo.state().running)
    }

    @Test
    fun manualPowerIsReportedAndAutoRestoresDefault() = runBlocking {
        demo.start()
        demo.setPower(55.0)
        assertEquals(55.0, demo.state().power, 0.001)
        demo.setPower(null)
        assertEquals(DemoController.DEMO_AUTO_POWER_PERCENT, demo.state().power, 0.001)
    }

    @Test
    fun powerOutOfRangeIsRejected() = runBlocking {
        demo.start()
        try {
            demo.setPower(150.0)
            fail("мощность выше 100 % должна отклоняться")
        } catch (e: ControllerException) {
            // ожидаемо
        }
    }

    @Test
    fun pauseStopsElapsedTimeUntilStop() = runBlocking {
        demo.start()
        now += 60_000L
        demo.pause()
        now += 60_000L
        val state = demo.state()
        assertTrue(state.paused)
        assertEquals(60, state.elapsedSec)
    }

    @Test
    fun pauseRequiresRunningProcess() = runBlocking {
        try {
            demo.pause()
            fail("пауза без процесса должна отклоняться")
        } catch (e: ControllerException) {
            // ожидаемо
        }
    }

    @Test
    fun postJsonMergesConfigAndGetReturnsIt() = runBlocking {
        demo.postJson("/api/config", JSONObject().put("hys", 2.5))
        assertEquals(2.5, demo.getJson("/api/config").getDouble("hys"), 0.001)
    }

    @Test
    fun postJsonToUnsupportedPathIsRejected() = runBlocking {
        try {
            demo.postJson("/api/unknown", JSONObject().put("x", 1))
            fail("запись в неизвестный раздел должна отклоняться")
        } catch (e: ControllerException) {
            // ожидаемо
        }
    }

    @Test
    fun eventsHaveEventsArray() = runBlocking {
        val events = demo.getJson("/api/events").getJSONArray("events")
        assertTrue(events.length() > 0)
    }
}
