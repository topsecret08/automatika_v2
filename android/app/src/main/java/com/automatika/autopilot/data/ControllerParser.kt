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

    /** /api/history/processes: массив процессов из базы. */
    fun processes(json: JSONArray): List<ProcessRecord> = (0 until json.length()).mapNotNull { index ->
        val item = json.optJSONObject(index) ?: return@mapNotNull null
        ProcessRecord(
            id = item.optLong("id", 0L),
            type = item.optInt("type", 0),
            startedMs = item.optLong("started_ms", 0L),
            endedMs = item.optLong("ended_ms", 0L),
            durationSec = item.optDouble("duration_sec", 0.0),
            headsMl = item.optDouble("heads_ml", 0.0),
            bodyMl = item.optDouble("body_ml", 0.0),
            tailsMl = item.optDouble("tails_ml", 0.0),
            rating = item.optInt("rating", 0),
            recipeId = item.optString("recipe_id", ""),
        )
    }

    /** /api/ai/status: аномалии, точность предсказаний, попадания кэша. */
    fun aiStatus(json: JSONObject): AiStatus {
        val anomaly = json.optJSONObject("anomaly") ?: JSONObject()
        return AiStatus(
            dbReady = json.optBoolean("db_ready", false),
            anomalyError = anomaly.optDouble("error", Double.NaN),
            anomalyThreshold = anomaly.optDouble("threshold", Double.NaN),
            isAnomaly = anomaly.optBoolean("is_anomaly", false),
            anomalySamples = anomaly.optLong("samples", 0L),
            actionAccuracy = json.optDouble("action_accuracy", Double.NaN),
            cacheHitRate = json.optDouble("cache_hit_rate", Double.NaN),
        )
    }

    /** /api/calib/state: короткие строки по датчикам, весам и клапанам. */
    fun calibLines(json: JSONObject): List<String> {
        val lines = mutableListOf<String>()
        json.optJSONObject("sensors")?.let { s ->
            lines += "Датчики: состояние ${s.optInt("state", 0)}, эталон ${num(s.optDouble("reference", Double.NaN), 1)} °C, " +
                "сохранено ${if (s.optBoolean("saved", false)) "да" else "нет"}"
            val error = s.optString("error", "")
            if (error.isNotBlank()) lines += "Ошибка датчиков: $error"
        }
        json.optJSONObject("scale")?.let { s ->
            lines += "Весы: ${if (s.optBoolean("ready", false)) "готовы" else "не готовы"}, " +
                "${if (s.optBoolean("stable", false)) "стабильно" else "нестабильно"}, " +
                "${num(s.optDouble("grams", Double.NaN), 1)} г, всего ${num(s.optDouble("total_ml", Double.NaN), 0)} мл"
        }
        json.optJSONArray("valves")?.let { valves ->
            for (i in 0 until valves.length()) {
                val v = valves.optJSONObject(i) ?: continue
                lines += "Клапан ${v.optInt("idx", i) + 1}: " +
                    "${if (v.optBoolean("active", false)) "калибровка идёт" else "калибровка не идёт"}, " +
                    "${num(v.optDouble("ml_per_hour", Double.NaN), 0)} мл/ч"
            }
        }
        return lines
    }

    private fun num(value: Double, digits: Int): String =
        if (value.isNaN()) "—" else String.format(java.util.Locale.US, "%.${digits}f", value)

    fun health(json: JSONObject): ControllerHealth = ControllerHealth(
        version = json.optString("version", "?"),
        uptimeSec = json.optLong("uptime_s", 0L),
    )
}
