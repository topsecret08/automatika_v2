package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.automatika.autopilot.data.ControllerParser
import com.automatika.autopilot.data.HelpArticle
import com.automatika.autopilot.data.SensorReading
import com.automatika.autopilot.data.WatchdogTask
import java.net.URLEncoder
import java.util.Locale

/** Что показывает экран диагностики. Ключи совпадают с пунктами меню. */
enum class DiagKind(val title: String) {
    OVERVIEW("Обзор системы"),
    SENSORS("Датчики DS18B20"),
    WATCHDOG("Watchdog"),
}

/** Статья справки: текст из /api/help. Заголовок берётся из пункта меню. */
@Composable
fun HelpCard(articleId: String, title: String, vm: AutopilotViewModel) {
    var article by remember { mutableStateOf<HelpArticle?>(null) }
    var status by remember { mutableStateOf("Загрузка…") }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            OutlinedButton(onClick = {
                vm.execute { api ->
                    val encoded = URLEncoder.encode(articleId, "UTF-8")
                    article = ControllerParser.help(api.getJson("/api/help?id=$encoded"))
                    status = ""
                }
            }) { Text("Обновить") }
            if (status.isNotBlank()) Text(status, style = MaterialTheme.typography.bodySmall)
            article?.let { Text(it.contentMd, style = MaterialTheme.typography.bodyMedium) }
        }
    }
}

/** Диагностика: обзор, датчики DS18B20 или watchdog. Только чтение. */
@Composable
fun DiagCard(kind: DiagKind, vm: AutopilotViewModel) {
    var lines by remember { mutableStateOf<List<String>>(emptyList()) }
    var status by remember { mutableStateOf("Нажмите «Обновить».") }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(kind.title, style = MaterialTheme.typography.titleMedium)
            OutlinedButton(onClick = {
                vm.execute { api ->
                    lines = when (kind) {
                        DiagKind.OVERVIEW -> {
                            val health = ControllerParser.health(api.getJson("/api/health"))
                            listOf("Версия прошивки: ${health.version}", "Аптайм: ${health.uptimeSec} с")
                        }
                        DiagKind.SENSORS -> ControllerParser.sensors(api.getArray("/api/diag/sensors")).map(::sensorLine)
                        DiagKind.WATCHDOG -> ControllerParser.watchdog(api.getArray("/api/diag/watchdog")).map(::watchdogLine)
                    }
                    status = if (lines.isEmpty()) "Данных нет." else ""
                }
            }) { Text("Обновить") }
            if (status.isNotBlank()) Text(status, style = MaterialTheme.typography.bodySmall)
            lines.forEach { Text(it, style = MaterialTheme.typography.bodySmall) }
        }
    }
}

private fun num(value: Double, digits: Int): String =
    if (value.isNaN()) "—" else String.format(Locale.US, "%.${digits}f", value)

internal fun sensorLine(s: SensorReading): String =
    "Датчик ${s.idx + 1}: ${num(s.temp, 2)} °C, шум ${num(s.noiseRms, 3)} °C, " +
        "показание ${s.ageMs} мс назад, адрес ${s.address}"

internal fun watchdogLine(t: WatchdogTask): String {
    val state = when {
        !t.enabled -> "выключена"
        t.ageMs > t.maxStaleMs -> "ЗАВИСЛА"
        else -> "в норме"
    }
    return "${t.name}: $state, без кормления ${t.ageMs} мс (лимит ${t.timeoutMs} мс), " +
        "перезапусков ${t.restarts}${if (t.critical) ", критичная" else ""}"
}
