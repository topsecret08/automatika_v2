package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
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
import org.json.JSONObject

/** Действия обслуживания и калибровок. Ключи совпадают с эндпоинтами прошивки. */
enum class MaintKind(val title: String) {
    RESET_AI("Сброс ИИ"),
    RESET_AUTOLEARN("Сброс автообучения"),
    RESET_TINYML("Сброс TinyML"),
    DB_BACKUP("Копия базы"),
    DB_VACUUM("Оптимизация базы"),
    DB_STATS("Статистика базы"),
    USB("Флешка USB"),
    SCALE("Весы"),
    SENSOR_CALIB("Калибровка датчиков"),
    VALVE_STATE("Калибровка клапанов"),
}

/** Экран обслуживания: одна карточка на действие. Сбросы и тара требуют подтверждения. */
@Composable
fun MaintenanceCard(kind: MaintKind, vm: AutopilotViewModel) {
    var status by remember { mutableStateOf("") }
    var calibLines by remember { mutableStateOf<List<String>>(emptyList()) }
    var confirmTarget by remember { mutableStateOf<(() -> Unit)?>(null) }
    var confirmText by remember { mutableStateOf("") }

    fun reset(target: String) = vm.execute { api ->
        api.postJson("/api/maint/reset", JSONObject().put("target", target))
        status = "Сброшено."
    }

    fun dbOp(op: String) = vm.execute { api ->
        val body = api.postJson("/api/maint/db", JSONObject().put("op", op))
        status = when (op) {
            "backup" -> "Копия создана: /sdcard/backups/db_auto.db"
            "vacuum" -> "База оптимизирована."
            else -> body.optString("text", "")
        }
    }

    fun refreshCalib() = vm.execute { api ->
        calibLines = ControllerParser.calibLines(api.getJson("/api/calib/state"))
        status = if (calibLines.isEmpty()) "Состояние пусто." else ""
    }

    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(kind.title, style = MaterialTheme.typography.titleMedium)
            when (kind) {
                MaintKind.RESET_AI -> {
                    Text("Сбрасывает модели предсказаний и аномалий. Историю процессов не трогает.", style = MaterialTheme.typography.bodySmall)
                    Button(onClick = {
                        confirmText = "Сбросить модели ИИ?"
                        confirmTarget = { reset("ai") }
                    }) { Text("Сбросить") }
                }
                MaintKind.RESET_AUTOLEARN -> {
                    Text("Сбрасывает накопленное автообучение.", style = MaterialTheme.typography.bodySmall)
                    Button(onClick = {
                        confirmText = "Сбросить автообучение?"
                        confirmTarget = { reset("autolearn") }
                    }) { Text("Сбросить") }
                }
                MaintKind.RESET_TINYML -> {
                    Text("Сбрасывает модель TinyML.", style = MaterialTheme.typography.bodySmall)
                    Button(onClick = {
                        confirmText = "Сбросить TinyML?"
                        confirmTarget = { reset("tinyml") }
                    }) { Text("Сбросить") }
                }
                MaintKind.DB_BACKUP -> OutlinedButton(onClick = { dbOp("backup") }) { Text("Создать копию") }
                MaintKind.DB_VACUUM -> OutlinedButton(onClick = { dbOp("vacuum") }) { Text("Оптимизировать") }
                MaintKind.DB_STATS -> OutlinedButton(onClick = { dbOp("stats") }) { Text("Показать статистику") }
                MaintKind.USB -> OutlinedButton(onClick = {
                    vm.execute { api ->
                        val mounted = api.getJson("/api/usb/status").optInt("mounted", 0) != 0
                        status = if (mounted) "Флешка подключена." else "Флешка не подключена."
                    }
                }) { Text("Проверить") }
                MaintKind.SCALE -> {
                    Text("Тара обнуляет вес и накопленный объём. Во время процесса не работает.", style = MaterialTheme.typography.bodySmall)
                    Button(onClick = {
                        confirmText = "Выполнить тару весов? Накопленный объём будет обнулён."
                        confirmTarget = {
                            vm.execute { api ->
                                api.postJson("/api/calib/scale/tare", JSONObject())
                                status = "Тара выполнена."
                            }
                        }
                    }) { Text("Тара") }
                    OutlinedButton(onClick = { refreshCalib() }) { Text("Обновить состояние") }
                }
                MaintKind.SENSOR_CALIB -> {
                    Text("Калибровка датчиков ждёт температуры и не греет куб. Здесь её можно отменить.", style = MaterialTheme.typography.bodySmall)
                    OutlinedButton(onClick = {
                        vm.execute { api ->
                            api.postJson("/api/calib/sensors/abort", JSONObject())
                            status = "Калибровка датчиков отменена."
                        }
                    }) { Text("Отменить калибровку") }
                    OutlinedButton(onClick = { refreshCalib() }) { Text("Обновить состояние") }
                }
                MaintKind.VALVE_STATE -> {
                    Text("Только состояние. Открытие клапанов из приложения пока не поддерживается.", style = MaterialTheme.typography.bodySmall)
                    OutlinedButton(onClick = { refreshCalib() }) { Text("Обновить состояние") }
                }
            }
            calibLines.forEach { Text(it, style = MaterialTheme.typography.bodySmall) }
            if (status.isNotBlank()) Text(status, style = MaterialTheme.typography.bodyMedium)
        }
    }

    confirmTarget?.let { action ->
        AlertDialog(
            onDismissRequest = { confirmTarget = null },
            text = { Text(confirmText) },
            confirmButton = {
                Button(onClick = {
                    confirmTarget = null
                    action()
                }) { Text("Да") }
            },
            dismissButton = {
                OutlinedButton(onClick = { confirmTarget = null }) { Text("Отмена") }
            },
        )
    }
}
