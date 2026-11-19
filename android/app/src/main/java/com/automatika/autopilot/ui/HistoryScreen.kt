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
import com.automatika.autopilot.data.AiStatus
import com.automatika.autopilot.data.ControllerParser
import com.automatika.autopilot.data.ProcessRecord
import java.util.Locale

/** Названия типов в порядке process_type_t (state.h): 0 — простой, 1..9 — режимы. */
private val PROCESS_TYPE_NAMES = listOf(
    "Простой", "Дистилляция", "Ректификация", "Пивоварение", "ПВК", "НБК",
    "Вакуумная перегонка", "Двойная перегонка", "Гэбриэл", "Брожение",
)

fun processTypeName(type: Int): String = PROCESS_TYPE_NAMES.getOrElse(type) { "Тип $type" }

private fun num(value: Double, digits: Int): String =
    if (value.isNaN()) "—" else String.format(Locale.US, "%.${digits}f", value)

private fun durationText(seconds: Double): String {
    val total = seconds.toLong().coerceAtLeast(0L)
    return "%d:%02d".format(Locale.US, total / 60, total % 60)
}

/** История процессов из базы контроллера, новые сверху. */
@Composable
fun HistoryCard(vm: AutopilotViewModel) {
    var rows by remember { mutableStateOf<List<ProcessRecord>>(emptyList()) }
    var status by remember { mutableStateOf("Нажмите «Обновить».") }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("История процессов", style = MaterialTheme.typography.titleMedium)
            OutlinedButton(onClick = {
                vm.execute { api ->
                    rows = ControllerParser.processes(api.getArray("/api/history/processes?limit=50"))
                    status = if (rows.isEmpty()) "Процессов нет (или база не подключена)." else ""
                }
            }) { Text("Обновить") }
            if (status.isNotBlank()) Text(status, style = MaterialTheme.typography.bodySmall)
            rows.forEach { r ->
                Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Text(
                        "${processTypeName(r.type)} · ${durationText(r.durationSec)}",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    Text(
                        "голова ${num(r.headsMl, 0)} мл · тело ${num(r.bodyMl, 0)} мл · хвост ${num(r.tailsMl, 0)} мл",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    if (r.recipeId.isNotBlank()) {
                        Text("рецепт: ${r.recipeId}", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }
    }
}

/** Состояние ИИ: аномалии, точность предсказаний действий, попадания кэша. */
@Composable
fun AiStatusCard(vm: AutopilotViewModel) {
    var status by remember { mutableStateOf<AiStatus?>(null) }
    var message by remember { mutableStateOf("Нажмите «Обновить».") }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Статус ИИ", style = MaterialTheme.typography.titleMedium)
            OutlinedButton(onClick = {
                vm.execute { api ->
                    status = ControllerParser.aiStatus(api.getJson("/api/ai/status"))
                    message = ""
                }
            }) { Text("Обновить") }
            if (message.isNotBlank()) Text(message, style = MaterialTheme.typography.bodySmall)
            status?.let { s ->
                Text("База данных: ${if (s.dbReady) "готова" else "не подключена"}")
                Text("Аномалия: ${if (s.isAnomaly) "да" else "нет"}")
                Text("Ошибка модели: ${num(s.anomalyError, 2)}, порог ${num(s.anomalyThreshold, 2)}")
                Text("Образцов для модели: ${s.anomalySamples}")
                Text("Точность предсказаний действий: ${percent(s.actionAccuracy)}")
                Text("Попадания кэша: ${percent(s.cacheHitRate)}")
            }
        }
    }
}

private fun percent(fraction: Double): String =
    if (fraction.isNaN()) "—" else "${num(fraction * 100.0, 0)} %"
