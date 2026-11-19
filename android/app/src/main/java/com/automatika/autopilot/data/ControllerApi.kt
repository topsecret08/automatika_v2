package com.automatika.autopilot.data

import org.json.JSONObject

/** Состояние контроллера, как его отдаёт GET /api/state. */
data class ControllerState(
    val cube: Double,
    val column: Double,
    val power: Double,
    val running: Boolean,
    val paused: Boolean,
    val safetyOk: Boolean,
    val estop: Boolean,
    val heads: Double,
    val body: Double,
    val tails: Double,
    val mode: String,
    val elapsedSec: Int,
)

data class ControllerHealth(val version: String, val uptimeSec: Long)

class ControllerException(message: String) : Exception(message)

/** Режимы контроллера: идентификатор для API и подпись для экрана. */
enum class ProcessMode(val id: String, val title: String) {
    DISTILL("distill", "Дистилляция"),
    RECTIFY("rectify", "Ректификация"),
    BEER("beer", "Пивоварение"),
    PVK("pvk", "ПВК"),
    NBK("nbk", "НБК"),
    VACUUM("vacuum", "Вакуумная перегонка"),
    DOUBLE("double", "Двойная перегонка"),
    GABRIEL("gabriel", "Гэбриэл"),
    FERMENT("ferment", "Брожение"),
}

/**
 * Один интерфейс для живого контроллера и демо-режима.
 * Общие JSON-методы дают доступ ко всем настройкам контроллера без отдельного кода на каждое поле.
 */
interface ControllerApi {
    suspend fun state(): ControllerState
    suspend fun health(): ControllerHealth
    suspend fun start(mode: String = "")
    suspend fun stop()
    suspend fun pause()
    /** null — автоматический режим мощности, иначе ручная мощность в процентах. */
    suspend fun setPower(percent: Double?)
    suspend fun resetSafety()
    suspend fun getJson(path: String): JSONObject
    suspend fun getArray(path: String): JSONArray
    suspend fun postJson(path: String, body: JSONObject): JSONObject
}
