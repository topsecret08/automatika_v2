package com.automatika.autopilot.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.automatika.autopilot.BuildConfig
import com.automatika.autopilot.data.AppUpdater
import com.automatika.autopilot.data.ReleaseInfo
import com.automatika.autopilot.data.UpdateLogic
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

enum class UpdatePhase { IDLE, CHECKING, UP_TO_DATE, AVAILABLE, DOWNLOADING, READY, ERROR }

data class UpdateState(
    val phase: UpdatePhase = UpdatePhase.IDLE,
    val current: String = BuildConfig.VERSION_NAME,
    val latest: String? = null,
    val progress: Int = 0,
    val message: String = "",
)

/** Проверка, скачивание и установка обновления приложения. Состояние для карточки на странице «Приложение». */
class UpdateViewModel(app: Application) : AndroidViewModel(app) {
    private val updater = AppUpdater(app)
    private var release: ReleaseInfo? = null
    private var downloaded: File? = null

    private val _state = MutableStateFlow(UpdateState())
    val state: StateFlow<UpdateState> = _state.asStateFlow()

    fun check() {
        if (_state.value.phase == UpdatePhase.CHECKING || _state.value.phase == UpdatePhase.DOWNLOADING) return
        _state.value = UpdateState(phase = UpdatePhase.CHECKING, message = "Проверка релиза…")
        viewModelScope.launch {
            runCatching { withContext(Dispatchers.IO) { updater.fetchLatest() } }
                .onSuccess { info ->
                    release = info
                    val newer = UpdateLogic.isNewer(info.tag, BuildConfig.VERSION_NAME)
                    _state.value = UpdateState(
                        phase = if (newer) UpdatePhase.AVAILABLE else UpdatePhase.UP_TO_DATE,
                        latest = info.tag,
                        message = if (newer) "Доступна версия ${info.tag}" else "Установлена последняя версия",
                    )
                }
                .onFailure { e -> fail("Проверка не удалась: ${e.message ?: e.javaClass.simpleName}") }
        }
    }

    /** Скачивает и проверяет APK, затем открывает установщик (или экран разрешения). */
    fun downloadAndInstall() {
        val info = release ?: return
        _state.value = _state.value.copy(phase = UpdatePhase.DOWNLOADING, progress = 0, message = "Загрузка…")
        viewModelScope.launch {
            runCatching {
                withContext(Dispatchers.IO) {
                    updater.downloadVerified(info) { p ->
                        _state.value = _state.value.copy(progress = p)
                    }
                }
            }.onSuccess { file ->
                downloaded = file
                _state.value = _state.value.copy(phase = UpdatePhase.READY, progress = 100, message = "Проверка SHA-256 пройдена")
                install()
            }.onFailure { e -> fail("Загрузка не удалась: ${e.message ?: e.javaClass.simpleName}") }
        }
    }

    /** Открывает установщик для уже проверенного файла. Без разрешения сначала ведёт в настройки. */
    fun install() {
        val file = downloaded ?: return
        if (!updater.canInstall()) {
            _state.value = _state.value.copy(message = "Разрешите установку из этого приложения и нажмите «Установить» снова")
            updater.openInstallPermission()
            return
        }
        runCatching { updater.install(file) }
            .onFailure { e -> fail("Не удалось открыть установщик: ${e.message ?: e.javaClass.simpleName}") }
    }

    private fun fail(message: String) {
        _state.value = _state.value.copy(phase = UpdatePhase.ERROR, message = message)
    }
}
