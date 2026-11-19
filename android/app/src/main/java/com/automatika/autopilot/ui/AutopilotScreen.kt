package com.automatika.autopilot.ui

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
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
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.automatika.autopilot.data.ControllerState
import com.automatika.autopilot.data.ProcessMode

/** Пункт «Приложение»: подключение, роль, журнал и ручная мощность. Есть только в приложении. */
private val APP_TILE = MenuItem(
    id = "app.page",
    icon = "📱",
    title = "Приложение и подключение",
    type = MenuType.SCREEN,
    colorCategory = 10,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AutopilotScreen(vm: AutopilotViewModel) {
    val ui by vm.ui.collectAsStateWithLifecycle()
    val role = ui.settings.role

    val stack = remember { mutableStateListOf<MenuItem>() }
    var editor by remember { mutableStateOf<MenuOutcome.Editor?>(null) }
    var appPage by remember { mutableStateOf(false) }
    var pending by remember { mutableStateOf<Pair<MenuItem, MenuOutcome>?>(null) }
    var notice by remember { mutableStateOf("") }

    val currentEditor = editor
    val atRoot = currentEditor == null && !appPage && stack.isEmpty()

    fun back() {
        when {
            currentEditor != null -> editor = null
            appPage -> appPage = false
            stack.isNotEmpty() -> stack.removeAt(stack.lastIndex)
        }
    }

    fun choose(item: MenuItem) {
        notice = ""
        if (item.id == APP_TILE.id) {
            appPage = true
            return
        }
        if (!MenuRules.isAvailable(item, role)) {
            notice = "«${item.title}» доступно с ролью: ${Roles.name(item.requiredRole)}."
            return
        }
        when (val outcome = MenuActions.resolve(item)) {
            MenuOutcome.Open -> stack.add(item)
            is MenuOutcome.Start, MenuOutcome.ResetAlarm -> pending = item to outcome
            is MenuOutcome.Editor -> editor = outcome
            MenuOutcome.OnDeviceOnly -> notice = "«${item.title}» пока доступно только на дисплее контроллера."
        }
    }

    BackHandler(enabled = !atRoot) { back() }

    val title = when {
        currentEditor != null -> currentEditor.title
        appPage -> "Приложение и подключение"
        stack.isNotEmpty() -> stack.last().title
        else -> "Главное меню"
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(title) },
                navigationIcon = {
                    if (!atRoot) TextButton(onClick = { back() }) { Text("←") }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(12.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            LinkBanner(ui)
            StatusStrip(ui, vm)
            if (notice.isNotBlank()) {
                Card(Modifier.fillMaxWidth()) {
                    Text(notice, Modifier.padding(12.dp), style = MaterialTheme.typography.bodyMedium)
                }
            }
            when {
                currentEditor != null -> JsonEditorCard(currentEditor.title, currentEditor.path, vm)
                appPage -> AppPage(ui, vm)
                stack.isEmpty() -> MenuTiles(ROOT_MENU + APP_TILE, role) { choose(it) }
                else -> MenuList(stack.last().children, role) { choose(it) }
            }
        }
    }

    pending?.let { (item, outcome) ->
        AlertDialog(
            onDismissRequest = { pending = null },
            title = { Text(item.title) },
            text = {
                Text(
                    when (outcome) {
                        is MenuOutcome.Start -> "Запустить процесс «${outcome.mode.title}»?"
                        else -> "Сбросить аварию? Проверьте, что E-STOP отпущен и уровень жидкости в норме."
                    },
                )
            },
            confirmButton = {
                Button(onClick = {
                    when (outcome) {
                        is MenuOutcome.Start -> vm.start(outcome.mode.id)
                        MenuOutcome.ResetAlarm -> vm.resetSafety()
                        else -> Unit
                    }
                    pending = null
                }) { Text("Да") }
            },
            dismissButton = {
                OutlinedButton(onClick = { pending = null }) { Text("Отмена") }
            },
        )
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
        Column(Modifier.padding(12.dp)) {
            Text(
                if (live) "Подключено к контроллеру (${ui.version})" else "ДЕМО-РЕЖИМ",
                style = MaterialTheme.typography.titleSmall,
            )
            if (ui.message.isNotBlank()) {
                Text(ui.message, style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}

/** Строка состояния как на главном экране дисплея: температуры, мощность, процесс, пауза и стоп. */
@Composable
private fun StatusStrip(ui: UiState, vm: AutopilotViewModel) {
    val state = ui.state
    val running = state?.running == true
    val alarm = state != null && (!state.safetyOk || state.estop)
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Куб ${fmt(state?.cube)} °C")
                Text("Царга ${fmt(state?.column)} °C")
                Text("Мощность ${fmt(state?.power)} %")
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text(processText(state, alarm), style = MaterialTheme.typography.bodyMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    OutlinedButton(
                        onClick = { vm.pause() },
                        enabled = !ui.busy && running && state?.paused != true,
                    ) { Text("Пауза") }
                    Button(onClick = { vm.stop() }, enabled = !ui.busy && running) { Text("Стоп") }
                }
            }
        }
    }
}

private fun processText(state: ControllerState?, alarm: Boolean): String = when {
    state == null -> "Нет данных"
    alarm -> "АВАРИЯ: сбросьте в меню «Сброс аварии»"
    !state.running -> "Процесс остановлен"
    state.paused -> "Пауза, ${state.elapsedSec} с"
    else -> "${ProcessMode.entries.firstOrNull { it.id == state.mode }?.title ?: state.mode}, ${state.elapsedSec} с"
}

/** Раздел «Приложение»: подключение, роль, ручная мощность и журнал. */
@Composable
private fun AppPage(ui: UiState, vm: AutopilotViewModel) {
    RoleCard(ui.settings) { role -> vm.saveSettings(ui.settings.copy(role = role)) }
    PowerSection(ui, vm)
    SettingsCard(settings = ui.settings, onSave = vm::saveSettings)
    EventsSection(vm)
}

@Composable
private fun RoleCard(settings: Settings, onRole: (Int) -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Роль", style = MaterialTheme.typography.titleMedium)
            Text(
                "Роль скрывает пункты меню, как на дисплее. Контроллер проверяет свои права отдельно.",
                style = MaterialTheme.typography.bodySmall,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf(Roles.OPERATOR, Roles.ENGINEER, Roles.ADMIN).forEach { role ->
                    if (settings.role == role) {
                        Button(onClick = { onRole(role) }) { Text(Roles.name(role)) }
                    } else {
                        OutlinedButton(onClick = { onRole(role) }) { Text(Roles.name(role)) }
                    }
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
            Text("Ручная мощность", style = MaterialTheme.typography.titleMedium)
            Text("Цель: ${"%.0f".format(target)} %")
            Slider(
                value = target,
                onValueChange = { target = it },
                valueRange = 0f..100f,
                steps = 99,
                enabled = running && !ui.busy,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { vm.setPower(target.toDouble()) }, enabled = running && !ui.busy) {
                    Text("Применить")
                }
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
            Text("Журнал событий", style = MaterialTheme.typography.titleMedium)
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
                    onSave(
                        settings.copy(
                            host = host.trim().ifBlank { Settings.DEFAULT_HOST },
                            apiKey = apiKey.trim(),
                            forceDemo = forceDemo,
                        ),
                    )
                },
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Сохранить") }
        }
    }
}

private fun fmt(value: Double?): String =
    if (value == null || value.isNaN()) "—" else "%.1f".format(value)
