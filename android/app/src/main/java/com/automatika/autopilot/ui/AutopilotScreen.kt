package com.automatika.autopilot.ui

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.automatika.autopilot.data.ControllerState
import com.automatika.autopilot.data.ProcessMode

/** Разделы приложения; null — главный экран. */
enum class Section(val title: String) {
    MODES("Режимы и процесс"),
    POWER("Мощность"),
    CONFIG("Настройки контроллера"),
    CONTROL("Регулирование"),
    HARDWARE("Оборудование"),
    EVENTS("Журнал событий"),
    CONNECTION("Подключение"),
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AutopilotScreen(vm: AutopilotViewModel) {
    val ui by vm.ui.collectAsStateWithLifecycle()
    var section by rememberSaveable { mutableStateOf<Section?>(null) }

    BackHandler(enabled = section != null) { section = null }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(section?.title ?: "Автоматика Autopilot") },
                navigationIcon = {
                    if (section != null) {
                        TextButton(onClick = { section = null }) { Text("←") }
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            LinkBanner(ui)
            when (section) {
                null -> Dashboard(ui, vm) { section = it }
                Section.MODES -> ModesSection(ui, vm)
                Section.POWER -> PowerSection(ui, vm)
                Section.CONFIG -> JsonEditorCard(Section.CONFIG.title, "/api/config", vm)
                Section.CONTROL -> JsonEditorCard(Section.CONTROL.title, "/api/control", vm)
                Section.HARDWARE -> JsonEditorCard(Section.HARDWARE.title, "/api/hardware", vm)
                Section.EVENTS -> EventsSection(vm)
                Section.CONNECTION -> SettingsCard(settings = ui.settings, onSave = vm::saveSettings)
            }
        }
    }
}

@Composable
private fun Dashboard(ui: UiState, vm: AutopilotViewModel, onOpen: (Section) -> Unit) {
    StatusCard(ui.state)
    ControlsCard(
        state = ui.state,
        busy = ui.busy,
        onStart = { vm.start() },
        onPause = vm::pause,
        onStop = vm::stop,
        onReset = vm::resetSafety,
    )
    if (ui.message.isNotBlank()) {
        Text(ui.message, style = MaterialTheme.typography.bodyMedium)
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Разделы", style = MaterialTheme.typography.titleMedium)
            Section.entries.forEach { item ->
                OutlinedButton(onClick = { onOpen(item) }, modifier = Modifier.fillMaxWidth()) {
                    Text(item.title)
                }
            }
        }
    }
}

@Composable
private fun ModesSection(ui: UiState, vm: AutopilotViewModel) {
    val state = ui.state
    val running = state?.running == true
    val alarm = state != null && (!state.safetyOk || state.estop)
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(
                "Текущий режим: ${ProcessMode.entries.firstOrNull { it.id == state?.mode }?.title ?: "—"}",
                style = MaterialTheme.typography.titleMedium,
            )
            val processText = when {
                !running -> "Процесс остановлен"
                state?.paused == true -> "Процесс идёт (пауза)"
                else -> "Процесс идёт"
            }
            Text(processText)
            ProcessMode.entries.forEach { mode ->
                Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(mode.title)
                    Button(
                        onClick = { vm.start(mode.id) },
                        enabled = !ui.busy && !running && !alarm,
                    ) { Text("Запустить") }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = vm::pause, enabled = !ui.busy && running && state?.paused != true) {
                    Text("Пауза")
                }
                OutlinedButton(onClick = vm::stop, enabled = !ui.busy && running) { Text("Стоп") }
                OutlinedButton(onClick = vm::resetSafety, enabled = !ui.busy && alarm) {
                    Text("Сброс аварии")
                }
            }
        }
    }
}

@Composable
private fun PowerSection(ui: UiState, vm: AutopilotViewModel) {
    val current = ui.state?.power?.toFloat() ?: 0f
    var target by remember { mutableFloatStateOf(current) }
    val running = ui.state?.running == true
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Мощность сейчас: ${"%.0f".format(current)} %", style = MaterialTheme.typography.titleMedium)
            Text("Цель: ${"%.0f".format(target)} %")
            Slider(
                value = target,
                onValueChange = { target = it },
                valueRange = 0f..100f,
                steps = 99,
                enabled = running && !ui.busy,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = { vm.setPower(target.toDouble()) },
                    enabled = running && !ui.busy,
                ) { Text("Применить") }
                OutlinedButton(onClick = { vm.setPower(null) }, enabled = running && !ui.busy) {
                    Text("Авто")
                }
            }
            if (!running) {
                Text("Мощность меняется только во время процесса.", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}

@Composable
private fun EventsSection(vm: AutopilotViewModel) {
    var lines by remember { mutableStateOf("Нажмите «Обновить».") }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = {
                vm.execute { api ->
                    val events = api.getJson("/api/events").optJSONArray("events")
                    lines = if (events == null || events.length() == 0) {
                        "Событий нет."
                    } else {
                        (0 until events.length()).joinToString("\n") { index ->
                            val item = events.getJSONObject(index)
                            "${item.optString("type")}  ${item.optString("msg")}"
                        }
                    }
                }
            }) { Text("Обновить") }
            Text(lines)
        }
    }
}

@Composable
private fun LinkBanner(ui: UiState) {
    val live = ui.link == Link.LIVE
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = androidx.compose.material3.CardDefaults.cardColors(
            containerColor = if (live) MaterialTheme.colorScheme.primaryContainer
            else MaterialTheme.colorScheme.tertiaryContainer,
        ),
    ) {
        Column(Modifier.padding(16.dp)) {
            Text(
                if (live) "Подключено к контроллеру (${ui.version})" else "ДЕМО-РЕЖИМ",
                style = MaterialTheme.typography.titleMedium,
            )
            if (ui.message.isNotBlank()) {
                Text(ui.message, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun StatusCard(state: ControllerState?) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Состояние", style = MaterialTheme.typography.titleMedium)
            if (state == null) {
                Text("Нет данных")
            } else {
                StateRows(state)
            }
        }
    }
}

@Composable
private fun StateRows(state: ControllerState) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Куб")
            Text("${fmt(state.cube)} °C")
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Царга")
            Text("${fmt(state.column)} °C")
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Мощность")
            Text("${fmt(state.power)} %")
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Головы / тело / хвосты")
            Text("${fmt(state.heads)} / ${fmt(state.body)} / ${fmt(state.tails)} мл")
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Процесс")
            Text(
                when {
                    !state.running -> "остановлен"
                    state.paused -> "пауза, ${state.elapsedSec} с"
                    else -> "идёт, ${state.elapsedSec} с"
                },
            )
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Безопасность")
            Text(if (state.safetyOk && !state.estop) "в норме" else "АВАРИЯ")
        }
    }
}

@Composable
private fun ControlsCard(
    state: ControllerState?,
    busy: Boolean,
    onStart: () -> Unit,
    onPause: () -> Unit,
    onStop: () -> Unit,
    onReset: () -> Unit,
) {
    val running = state?.running == true
    val alarm = state != null && (!state.safetyOk || state.estop)
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Управление", style = MaterialTheme.typography.titleMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = onStart, enabled = !busy && !running && !alarm) { Text("Старт") }
                OutlinedButton(onClick = onPause, enabled = !busy && running && state?.paused != true) {
                    Text("Пауза")
                }
                OutlinedButton(onClick = onStop, enabled = !busy && running) { Text("Стоп") }
                OutlinedButton(onClick = onReset, enabled = !busy && alarm) { Text("Сброс аварии") }
            }
        }
    }
}

@Composable
private fun SettingsCard(settings: Settings, onSave: (Settings) -> Unit) {
    var host by remember(settings) { mutableStateOf(settings.host) }
    var apiKey by remember(settings) { mutableStateOf(settings.apiKey) }
    var forceDemo by remember(settings) { mutableStateOf(settings.forceDemo) }

    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Подключение", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(
                value = host,
                onValueChange = { host = it },
                label = { Text("Адрес контроллера") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = apiKey,
                onValueChange = { apiKey = it },
                label = { Text("API-ключ") },
                singleLine = true,
                visualTransformation = PasswordVisualTransformation(),
                modifier = Modifier.fillMaxWidth(),
            )
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Только демо-режим")
                Switch(checked = forceDemo, onCheckedChange = { forceDemo = it })
            }
            Button(
                onClick = {
                    onSave(Settings(host.trim().ifBlank { Settings.DEFAULT_HOST }, apiKey.trim(), forceDemo))
                },
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Сохранить") }
        }
    }
}

private fun fmt(value: Double): String =
    if (value.isNaN()) "—" else "%.1f".format(value)
