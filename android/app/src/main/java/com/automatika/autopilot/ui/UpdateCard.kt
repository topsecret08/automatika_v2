package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel

/** Обновление приложения из GitHub Releases: проверка, загрузка с проверкой SHA-256, установка. */
@Composable
fun UpdateCard(vm: UpdateViewModel = viewModel()) {
    val s by vm.state.collectAsState()
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Обновление приложения", style = MaterialTheme.typography.titleMedium)
            Text(
                "Установлена: ${s.current}" + (s.latest?.let { " • Последняя: $it" } ?: ""),
                style = MaterialTheme.typography.bodyMedium,
            )
            if (s.phase == UpdatePhase.DOWNLOADING) {
                LinearProgressIndicator(progress = { s.progress / 100f }, modifier = Modifier.fillMaxWidth())
            }
            if (s.message.isNotBlank()) {
                Text(s.message, style = MaterialTheme.typography.bodySmall)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(
                    onClick = { vm.check() },
                    enabled = s.phase != UpdatePhase.CHECKING && s.phase != UpdatePhase.DOWNLOADING,
                ) { Text("Проверить") }
                if (s.phase == UpdatePhase.AVAILABLE || s.phase == UpdatePhase.ERROR) {
                    Button(onClick = { vm.downloadAndInstall() }) { Text("Установить") }
                }
                if (s.phase == UpdatePhase.READY) {
                    Button(onClick = { vm.install() }) { Text("Открыть установщик") }
                }
            }
        }
    }
}
