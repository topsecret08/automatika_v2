package com.automatika.autopilot

import com.automatika.autopilot.data.ControllerException
import com.automatika.autopilot.data.DemoController
import kotlinx.coroutines.runBlocking
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
}
