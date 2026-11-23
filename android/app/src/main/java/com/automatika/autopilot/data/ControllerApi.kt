package com.automatika.autopilot.data

import org.json.JSONArray
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

/** Процесс из истории (/api/history/processes). Времена в миллисекундах от старта контроллера. */
data class ProcessRecord(
    val id: Long,
    val type: Int,
    val startedMs: Long,
    val endedMs: Long,
    val durationSec: Double,
    val headsMl: Double,
    val bodyMl: Double,
    val tailsMl: Double,
    val rating: Int,
    val recipeId: String,
)

/** Состояние ИИ (/api/ai/status). NaN, если контроллер не прислал значение. */
data class AiStatus(
    val dbReady: Boolean,
    val anomalyError: Double,
    val anomalyThreshold: Double,
    val isAnomaly: Boolean,
    val anomalySamples: Long,
    val actionAccuracy: Double,
    val cacheHitRate: Double,
)

/** Рецепт в списке (/api/recipes). */
data class RecipeSummary(val id: String, val name: String, val category: String, val description: String)

/** Полный рецепт (/api/recipe?id=...): ингредиенты и шаги в Markdown. */
data class RecipeDetail(
    val id: String,
    val name: String,
    val category: String,
    val description: String,
    val ingredients: String,
    val stepsMd: String,
)

/** Тумблер меню контроллера. key совпадает с nvs_key в menu_tree.c. */
data class ToggleSetting(val key: String, val title: String, val value: Boolean, val editable: Boolean)

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
