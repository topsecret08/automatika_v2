package com.automatika.autopilot.data

/** Состояние контроллера, как его отдаёт GET /api/state. */
data class ControllerState(
    val cube: Double,
    val column: Double,
    val power: Double,
    val running: Boolean,
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

/** Один интерфейс для живого контроллера и демо-режима. */
interface ControllerApi {
    suspend fun state(): ControllerState
    suspend fun health(): ControllerHealth
    suspend fun start()
    suspend fun stop()
    suspend fun resetSafety()
}
