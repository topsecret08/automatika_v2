package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.json.JSONArray
import org.json.JSONObject

/**
 * Редактор любого JSON-документа контроллера (например, /api/config).
 * Поля строятся по ответу контроллера: логические значения — переключатель,
 * числа и строки — текстовое поле. Вложенные объекты показываются только для просмотра.
 * На контроллер отправляются только изменённые ключи.
 */
@Composable
fun JsonEditorCard(title: String, path: String, vm: AutopilotViewModel) {
    var original by remember(path) { mutableStateOf<JSONObject?>(null) }
    val texts = remember(path) { mutableStateMapOf<String, String>() }
    var status by remember(path) { mutableStateOf("") }

    fun load() {
        vm.execute { api ->
            val json = api.getJson(path)
            texts.clear()
            json.keys().forEach { key -> texts[key] = json.get(key).toString() }
            original = json
        }
    }

    fun save() {
        val source = original ?: return
        val body = JSONObject()
        for (key in source.keys().asSequence().toList()) {
            val old = source.get(key)
            if (isReadOnly(old) || FieldNames.isReadOnly(key)) continue
            val text = texts[key] ?: continue
            if (text == old.toString()) continue
            val value: Any? = when (old) {
                is Boolean -> text.trim().toBooleanStrictOrNull()
                is Int -> text.trim().toIntOrNull()
                is Long -> text.trim().toLongOrNull()
                is Double -> text.trim().replace(',', '.').toDoubleOrNull()
                is String -> text
                else -> null
            }
            if (value == null) {
                status = "Неверное значение: $key"
                return
            }
            body.put(key, value)
        }
        if (body.length() == 0) {
            status = "Нет изменений"
            return
        }
        vm.execute { api ->
            api.postJson(path, body)
            status = "Сохранено: ${body.length()} поле(й)"
            load()
        }
    }

    LaunchedEffect(path) { load() }

    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(title, style = MaterialTheme.typography.titleMedium)
                OutlinedButton(onClick = { load() }) { Text("Обновить") }
            }

            val doc = original
            if (doc == null) {
                Text("Нет данных. Нажмите «Обновить».")
            } else {
                for (key in doc.keys().asSequence().toList().sorted()) {
                    val old = doc.get(key)
                    when {
                        isReadOnly(old) || FieldNames.isReadOnly(key) ->
                            Text("${FieldNames.label(key)}: ${texts[key].orEmpty()} (только чтение)")
                        old is Boolean -> Row(
                            Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(FieldNames.label(key), Modifier.padding(end = 8.dp))
                            Switch(
                                checked = texts[key] == "true",
                                onCheckedChange = { texts[key] = it.toString() },
                            )
                        }
                        else -> OutlinedTextField(
                            value = texts[key].orEmpty(),
                            onValueChange = { texts[key] = it },
                            label = { Text(FieldNames.label(key)) },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Button(onClick = { save() }) { Text("Сохранить") }
                    Text(
                        status,
                        Modifier.padding(start = 12.dp),
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
        }
    }
}

private fun isReadOnly(value: Any?): Boolean =
    value is JSONObject || value is JSONArray || value == JSONObject.NULL
