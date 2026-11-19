package com.automatika.autopilot.data

import kotlin.math.max
import kotlin.math.min

/**
 * Демо-режим: имитирует процесс дистилляции, когда контроллер не подключён.
 * Ничего не отправляет по сети; значения рассчитываются по времени работы.
 */
class DemoController(
    private val clock: () -> Long = { System.currentTimeMillis() },
) : ControllerApi {

    private var accumulatedMs = 0L
    private var runStartMs: Long? = null
    private var safetyOk = true
    private val startedAtMs = clock()

    private fun elapsedMs(): Long =
        accumulatedMs + (runStartMs?.let { clock() - it } ?: 0L)

    override suspend fun state(): ControllerState {
        val running = runStartMs != null
        val t = elapsedMs() / 1000.0
        val cube = if (running) AMBIENT_C + min(t * CUBE_RATE_PER_SEC, CUBE_MAX_C - AMBIENT_C) else AMBIENT_C
        val column = if (running) max(AMBIENT_C, min(cube - COLUMN_OFFSET_C, COLUMN_MAX_C)) else AMBIENT_C
        return ControllerState(
            cube = cube,
            column = column,
            power = if (running) DEMO_POWER_PERCENT else 0.0,
            running = running,
            safetyOk = safetyOk,
            estop = false,
            heads = if (running) min(t * HEADS_RATE_PER_SEC, HEADS_MAX_ML) else 0.0,
            body = if (running && t > BODY_START_SEC) min((t - BODY_START_SEC) * BODY_RATE_PER_SEC, BODY_MAX_ML) else 0.0,
            tails = 0.0,
            mode = "distill",
            elapsedSec = t.toInt(),
        )
    }

    override suspend fun health(): ControllerHealth =
        ControllerHealth(version = "демо", uptimeSec = (clock() - startedAtMs) / 1000)

    override suspend fun start() {
        if (!safetyOk) throw ControllerException("авария: сначала сбросьте аварию")
        if (runStartMs == null) runStartMs = clock()
    }

    override suspend fun stop() {
        runStartMs?.let { accumulatedMs += clock() - it }
        runStartMs = null
    }

    override suspend fun resetSafety() {
        if (runStartMs != null) throw ControllerException("сначала остановите процесс")
        safetyOk = true
    }

    /** Имитация аварии для демонстрации: останавливает процесс и блокирует запуск. */
    suspend fun triggerAlarm() {
        stop()
        safetyOk = false
    }

    companion object {
        const val AMBIENT_C = 21.0
        const val CUBE_MAX_C = 96.0
        const val CUBE_RATE_PER_SEC = 0.3
        const val COLUMN_OFFSET_C = 18.0
        const val COLUMN_MAX_C = 80.0
        const val DEMO_POWER_PERCENT = 40.0
        const val HEADS_RATE_PER_SEC = 0.2
        const val HEADS_MAX_ML = 120.0
        const val BODY_START_SEC = 300.0
        const val BODY_RATE_PER_SEC = 0.8
        const val BODY_MAX_ML = 1500.0
    }
}
