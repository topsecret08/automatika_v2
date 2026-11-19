package com.automatika.autopilot.data

import org.json.JSONObject

/** Разбор JSON контроллера. Отсутствующие поля получают безопасные значения. */
object ControllerParser {

    fun state(json: JSONObject): ControllerState = ControllerState(
        cube = json.optDouble("cube", Double.NaN),
        column = json.optDouble("col", Double.NaN),
        power = json.optDouble("power", 0.0),
        running = json.optInt("running", 0) != 0,
        safetyOk = json.optInt("safety", 1) != 0,
        estop = json.optInt("estop", 0) != 0,
        heads = json.optDouble("heads", 0.0),
        body = json.optDouble("body", 0.0),
        tails = json.optDouble("tails", 0.0),
        mode = json.optString("mode", ""),
        elapsedSec = json.optInt("elapsed", 0),
    )

    fun health(json: JSONObject): ControllerHealth = ControllerHealth(
        version = json.optString("version", "?"),
        uptimeSec = json.optLong("uptime_s", 0L),
    )
}
