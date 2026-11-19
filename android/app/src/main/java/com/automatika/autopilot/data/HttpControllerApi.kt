package com.automatika.autopilot.data

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/**
 * Клиент HTTP API контроллера Automatika (см. docs/developer/API_ROUTES.md).
 * Ключ передаётся в заголовке X-API-Key.
 */
class HttpControllerApi(
    host: String,
    private val apiKey: String,
) : ControllerApi {

    private val base: String = normalize(host)

    override suspend fun state(): ControllerState =
        ControllerParser.state(JSONObject(request("GET", "/api/state", null)))

    override suspend fun health(): ControllerHealth =
        ControllerParser.health(JSONObject(request("GET", "/api/health", null)))

    override suspend fun start(mode: String) {
        val body = if (mode.isBlank()) "{}" else JSONObject().put("mode", mode).toString()
        request("POST", "/api/start", body)
    }

    override suspend fun stop() {
        request("POST", "/api/stop", "{}")
    }

    override suspend fun pause() {
        request("POST", "/api/pause", "{}")
    }

    override suspend fun setPower(percent: Double?) {
        val body = if (percent == null) {
            JSONObject().put("mode", "auto")
        } else {
            JSONObject().put("power", percent)
        }
        request("POST", "/api/power", body.toString())
    }

    override suspend fun resetSafety() {
        request("POST", "/api/safety", "{}")
    }

    override suspend fun getJson(path: String): JSONObject =
        JSONObject(request("GET", path, null).ifBlank { "{}" })

    override suspend fun postJson(path: String, body: JSONObject): JSONObject =
        JSONObject(request("POST", path, body.toString()).ifBlank { "{}" })

    private suspend fun request(method: String, path: String, body: String?): String =
        withContext(Dispatchers.IO) {
            val connection = URL(base + path).openConnection() as HttpURLConnection
            try {
                connection.requestMethod = method
                connection.connectTimeout = CONNECT_TIMEOUT_MS
                connection.readTimeout = READ_TIMEOUT_MS
                connection.setRequestProperty("Accept", "application/json")
                if (apiKey.isNotBlank()) connection.setRequestProperty("X-API-Key", apiKey)
                if (body != null) {
                    connection.doOutput = true
                    connection.setRequestProperty("Content-Type", "application/json")
                    connection.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
                }
                val code = connection.responseCode
                val stream = if (code in 200..299) connection.inputStream else connection.errorStream
                val text = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
                if (code !in 200..299) {
                    throw ControllerException(
                        when (code) {
                            401 -> "неверный API-ключ"
                            403 -> "недостаточно прав (роль)"
                            409 -> "контроллер не готов: ${errorText(text)}"
                            else -> "HTTP $code: ${errorText(text)}"
                        }
                    )
                }
                text
            } catch (e: IOException) {
                throw ControllerException("нет связи с $base")
            } finally {
                connection.disconnect()
            }
        }

    private fun errorText(raw: String): String =
        runCatching { JSONObject(raw).optString("error", raw) }.getOrDefault(raw)

    companion object {
        private const val CONNECT_TIMEOUT_MS = 3000
        private const val READ_TIMEOUT_MS = 5000

        fun normalize(host: String): String {
            val trimmed = host.trim().trimEnd('/')
            return if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
                trimmed
            } else {
                "http://$trimmed"
            }
        }
    }
}
