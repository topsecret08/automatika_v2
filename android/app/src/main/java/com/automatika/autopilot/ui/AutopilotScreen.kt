package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.automatika.autopilot.data.ControllerState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AutopilotScreen(vm: AutopilotViewModel) {
    val ui by vm.ui.collectAsStateWithLifecycle()

    Scaffold(
        topBar = { TopAppBar(title = { Text("Автоматика Autopilot") }) },
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            LinkBanner(ui)
            StatusCard(ui.state)
            ControlsCard(
                state = ui.state,
                busy = ui.busy,
                onStart = vm::start,
                onStop = vm::stop,
                onReset = vm::resetSafety,
            )
            if (!ui.settings.forceDemo && ui.link == Link.DEMO) {
                Text(
                    "Чтобы подключиться к контроллеру, введите его адрес в настройках.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            SettingsCard(settings = ui.settings, onSave = vm::saveSettings)
            if (ui.link == Link.DEMO) {
                OutlinedButton(onClick = vm::simulateAlarm, modifier = Modifier.fillMaxWidth()) {
                    Text("Демо: имитировать аварию")
                }
            }
        }
    }
}

@Composable
private fun LinkBanner(ui: UiState) {
    val live = ui.link == Link.LIVE
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
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
                Text(if (state.running) "идёт, ${state.elapsedSec} с" else "остановлен")
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
            Text("Настройки", style = MaterialTheme.typography.titleMedium)
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
