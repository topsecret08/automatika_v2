package com.automatika.autopilot.ui

import android.content.Context
import android.content.SharedPreferences
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.automatika.autopilot.data.ControllerApi
import com.automatika.autopilot.data.ControllerState
import com.automatika.autopilot.data.DemoController
import com.automatika.autopilot.data.HttpControllerApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

data class Settings(
    val host: String = DEFAULT_HOST,
    val apiKey: String = "",
    val forceDemo: Boolean = false,
) {
    companion object {
        const val DEFAULT_HOST = "automatika.local"
    }
}

/** LIVE — контроллер отвечает; DEMO — показываются симулированные данные. */
enum class Link { LIVE, DEMO }

data class UiState(
    val settings: Settings = Settings(),
    val link: Link = Link.DEMO,
    val state: ControllerState? = null,
    val version: String = "",
    val message: String = "",
    val busy: Boolean = false,
)

/** Хранит настройки подключения между запусками приложения. */
class SettingsStore(context: Context) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("autopilot", Context.MODE_PRIVATE)

    fun load(): Settings = Settings(
        host = prefs.getString(KEY_HOST, Settings.DEFAULT_HOST) ?: Settings.DEFAULT_HOST,
        apiKey = prefs.getString(KEY_API_KEY, "") ?: "",
        forceDemo = prefs.getBoolean(KEY_FORCE_DEMO, false),
    )

    fun save(settings: Settings) {
        prefs.edit()
            .putString(KEY_HOST, settings.host)
            .putString(KEY_API_KEY, settings.apiKey)
            .putBoolean(KEY_FORCE_DEMO, settings.forceDemo)
            .apply()
    }

    private companion object {
        const val KEY_HOST = "host"
        const val KEY_API_KEY = "api_key"
        const val KEY_FORCE_DEMO = "force_demo"
    }
}

class AutopilotViewModel(
    private val store: SettingsStore,
    private val demo: DemoController = DemoController(),
    private val live: (Settings) -> ControllerApi = { HttpControllerApi(it.host, it.apiKey) },
) : ViewModel() {

    private val _ui = MutableStateFlow(UiState(settings = store.load()))
    val ui: StateFlow<UiState> = _ui.asStateFlow()

    private var failures = 0

    init {
        viewModelScope.launch {
            while (true) {
                poll()
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    fun saveSettings(settings: Settings) {
        store.save(settings)
        failures = 0
        _ui.update { it.copy(settings = settings) }
        viewModelScope.launch { poll() }
    }

    fun start() = command { it.start() }
    fun stop() = command { it.stop() }
    fun resetSafety() = command { it.resetSafety() }

    /** Демо-режим: имитация аварии, чтобы проверить блокировку запуска. */
    fun simulateAlarm() {
        viewModelScope.launch {
            demo.triggerAlarm()
            poll()
        }
    }

    private suspend fun poll() {
        val settings = _ui.value.settings
        if (settings.forceDemo) {
            failures = 0
            showDemo("Демо-режим включён в настройках. Данные симулированы.")
            return
        }
        try {
            val api = live(settings)
            val state = api.state()
            failures = 0
            val version = runCatching { api.health().version }.getOrDefault("")
            _ui.update { it.copy(link = Link.LIVE, state = state, version = version, message = "") }
        } catch (e: Exception) {
            failures++
            if (failures >= FAILURES_BEFORE_DEMO) {
                showDemo("Контроллер не подключён (${e.message ?: "нет связи"}). Показан демо-режим.")
            } else {
                _ui.update { it.copy(message = "Нет связи с контроллером, повторяем…") }
            }
        }
    }

    private suspend fun showDemo(message: String) {
        val state = demo.state()
        _ui.update {
            it.copy(link = Link.DEMO, state = state, version = "демо", message = message)
        }
    }

    private fun command(action: suspend (ControllerApi) -> Unit) {
        viewModelScope.launch {
            val current = _ui.value
            val api: ControllerApi = if (current.link == Link.LIVE && !current.settings.forceDemo) {
                live(current.settings)
            } else {
                demo
            }
            _ui.update { it.copy(busy = true) }
            try {
                action(api)
                _ui.update { it.copy(message = "") }
            } catch (e: Exception) {
                _ui.update { it.copy(message = "Команда не выполнена: ${e.message}") }
            } finally {
                _ui.update { it.copy(busy = false) }
            }
            poll()
        }
    }

    private companion object {
        const val POLL_INTERVAL_MS = 2000L
        const val FAILURES_BEFORE_DEMO = 2
    }
}

class AutopilotViewModelFactory(private val context: Context) : ViewModelProvider.Factory {
    @Suppress("UNCHECKED_CAST")
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        return AutopilotViewModel(SettingsStore(context.applicationContext)) as T
    }
}
