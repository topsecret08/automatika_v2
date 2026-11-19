package com.automatika.autopilot.data

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.max
import kotlin.math.min

/**
 * Демо-режим: имитирует процесс, когда контроллер не подключён.
 * Ничего не отправляет по сети: состояние и настройки хранятся в памяти приложения.
 */
class DemoController(
    private val clock: () -> Long = { System.currentTimeMillis() },
) : ControllerApi {

    private var accumulatedMs = 0L
    private var runStartMs: Long? = null
    /** Пауза действует до остановки процесса, как и на контроллере. */
    private var pausedAtMs: Long? = null
    private var safetyOk = true
    private var mode = ProcessMode.DISTILL.id
    private var manualPower: Double? = null
    private val startedAtMs = clock()

    private val documents: MutableMap<String, JSONObject> = mutableMapOf(
        PATH_CONFIG to JSONObject(mapOf(
            "ctrl_mode" to 0, "hys" to 1.0, "out_dev" to 0, "rate_mlmin" to 60,
            "flow_mlmin" to 60, "steps_per_ml" to 1120, "regulator" to 0,
            "reg_gpio" to -1, "reg_freq" to 5000, "reg_resolution" to 10, "reg_inverted" to 0,
        )),
        PATH_CONTROL to JSONObject(mapOf(
            "ctrl_mode" to 0, "hys" to 1.0, "rate_mlmin" to 60, "flow_mlmin" to 60,
        )),
        PATH_HARDWARE to JSONObject(mapOf(
            "demo" to true, "cube_role" to 0, "column_role" to 1, "pressure" to false,
        )),
    )

    /** Время работы: во время паузы не растёт. */
    private fun elapsedMs(now: Long = clock()): Long {
        val start = runStartMs ?: return accumulatedMs
        val effectiveEnd = pausedAtMs ?: now
        return accumulatedMs + (effectiveEnd - start).coerceAtLeast(0L)
    }

    override suspend fun state(): ControllerState {
        val running = runStartMs != null
        val t = elapsedMs() / 1000.0
        val cube = if (running) AMBIENT_C + min(t * CUBE_RATE_PER_SEC, CUBE_MAX_C - AMBIENT_C) else AMBIENT_C
        val column = if (running) max(AMBIENT_C, min(cube - COLUMN_OFFSET_C, COLUMN_MAX_C)) else AMBIENT_C
        val power = when {
            !running -> 0.0
            manualPower != null -> manualPower!!
            else -> DEMO_AUTO_POWER_PERCENT
        }
        return ControllerState(
            cube = cube,
            column = column,
            power = power,
            running = running,
            paused = running && pausedAtMs != null,
            safetyOk = safetyOk,
            estop = false,
            heads = if (running) min(t * HEADS_RATE_PER_SEC, HEADS_MAX_ML) else 0.0,
            body = if (running && t > BODY_START_SEC) min((t - BODY_START_SEC) * BODY_RATE_PER_SEC, BODY_MAX_ML) else 0.0,
            tails = 0.0,
            mode = mode,
            elapsedSec = t.toInt(),
        )
    }

    override suspend fun health(): ControllerHealth =
        ControllerHealth(version = "демо", uptimeSec = (clock() - startedAtMs) / 1000)

    override suspend fun start(mode: String) {
        if (!safetyOk) throw ControllerException("авария: сначала сбросьте аварию")
        if (runStartMs != null) return
        if (mode.isNotBlank()) {
            val known = ProcessMode.entries.firstOrNull { it.id == mode }
                ?: throw ControllerException("неизвестный режим: $mode")
            this.mode = known.id
        }
        runStartMs = clock()
        pausedAtMs = null
    }

    override suspend fun stop() {
        accumulatedMs = elapsedMs()
        runStartMs = null
        pausedAtMs = null
    }

    override suspend fun pause() {
        if (runStartMs == null) throw ControllerException("процесс не запущен")
        if (pausedAtMs == null) pausedAtMs = clock()
    }

    override suspend fun setPower(percent: Double?) {
        if (percent == null) {
            manualPower = null
            return
        }
        if (runStartMs == null) throw ControllerException("процесс не запущен")
        if (!percent.isFinite() || percent < 0.0 || percent > 100.0) {
            throw ControllerException("мощность должна быть от 0 до 100 %")
        }
        manualPower = percent
    }

    override suspend fun resetSafety() {
        if (runStartMs != null) throw ControllerException("сначала остановите процесс")
        safetyOk = true
    }

    override suspend fun getJson(path: String): JSONObject =
        JSONObject(documents[path]?.toString() ?: throw ControllerException("демо: нет данных для $path"))

    override suspend fun getArray(path: String): JSONArray {
        if (path == PATH_EVENTS) {
            return JSONArray().put(JSONObject().put("ts_ms", 0).put("type", 2).put("name", "INFO").put("msg", "Демо-режим"))
        }
        throw ControllerException("демо: нет массива для $path")
    }

    override suspend fun postJson(path: String, body: JSONObject): JSONObject {
        val doc = documents[path] ?: throw ControllerException("демо: запись в $path не поддерживается")
        val keys = body.keys()
        while (keys.hasNext()) {
            val key = keys.next()
            doc.put(key, body.get(key))
        }
        return JSONObject().put("status", "ok")
    }

    /** Имитация аварии для демонстрации: останавливает процесс и блокирует запуск. */
    suspend fun triggerAlarm() {
        stop()
        safetyOk = false
    }

    companion object {
        const val PATH_CONFIG = "/api/config"
        const val PATH_CONTROL = "/api/control"
        const val PATH_HARDWARE = "/api/hardware"
        const val PATH_EVENTS = "/api/events"

        const val AMBIENT_C = 21.0
        const val CUBE_MAX_C = 96.0
        const val CUBE_RATE_PER_SEC = 0.3
        const val COLUMN_OFFSET_C = 18.0
        const val COLUMN_MAX_C = 80.0
        const val DEMO_AUTO_POWER_PERCENT = 40.0
        const val HEADS_RATE_PER_SEC = 0.2
        const val HEADS_MAX_ML = 120.0
        const val BODY_START_SEC = 300.0
        const val BODY_RATE_PER_SEC = 0.8
        const val BODY_MAX_ML = 1500.0
    }
}
