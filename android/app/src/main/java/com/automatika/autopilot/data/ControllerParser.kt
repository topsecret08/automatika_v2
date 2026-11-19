package com.automatika.autopilot.data

import org.json.JSONArray
import org.json.JSONObject

/** Разбор JSON контроллера. Отсутствующие поля получают безопасные значения. */
object ControllerParser {

    fun state(json: JSONObject): ControllerState = ControllerState(
        cube = json.optDouble("cube", Double.NaN),
        column = json.optDouble("col", Double.NaN),
        power = json.optDouble("power", 0.0),
        running = json.optInt("running", 0) != 0,
        paused = json.optInt("paused", 0) != 0,
        safetyOk = json.optInt("safety", 1) != 0,
        estop = json.optInt("estop", 0) != 0,
        heads = json.optDouble("heads", 0.0),
        body = json.optDouble("body", 0.0),
        tails = json.optDouble("tails", 0.0),
        mode = json.optString("mode", ""),
        elapsedSec = json.optInt("elapsed", 0),
    )

    /** /api/events отдаёт массив {ts_ms, type, name, msg}; выводим "имя  текст". */
    fun events(json: JSONArray): List<String> = (0 until json.length()).mapNotNull { index ->
        val item = json.optJSONObject(index) ?: return@mapNotNull null
        val name = item.optString("name", "")
        val msg = item.optString("msg", "")
        listOf(name, msg).filter { it.isNotBlank() }.joinToString("  ").ifBlank { null }
    }

    /** /api/menu/toggles: тумблеры меню контроллера (ключ, подпись, значение, можно ли менять). */
    fun toggles(json: JSONArray): List<ToggleSetting> = (0 until json.length()).mapNotNull { index ->
        val item = json.optJSONObject(index) ?: return@mapNotNull null
        ToggleSetting(
            key = item.optString("key", ""),
            title = item.optString("title", ""),
            value = item.optInt("value", 0) != 0,
            editable = item.optBoolean("editable", false),
        ).takeIf { it.key.isNotBlank() }
    }

    fun health(json: JSONObject): ControllerHealth = ControllerHealth(
        version = json.optString("version", "?"),
        uptimeSec = json.optLong("uptime_s", 0L),
    )
}
