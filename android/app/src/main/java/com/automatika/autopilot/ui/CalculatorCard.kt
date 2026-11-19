package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt

/** Экран калькулятора: ползунки и результат, пересчёт на лету. Без связи с контроллером. */
@Composable
fun CalculatorCard(kind: CalcKind) {
    val values = remember(kind) {
        mutableStateListOf<Double>().apply { addAll(kind.sliders.map { it.default }) }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            kind.sliders.forEachIndexed { index, slider ->
                Text("${slider.label}: ${formatNumber(values[index], decimals(slider.step))}${slider.unit}")
                Slider(
                    value = values[index].toFloat(),
                    onValueChange = { raw -> values[index] = snap(raw.toDouble(), slider) },
                    valueRange = slider.min.toFloat()..slider.max.toFloat(),
                    steps = (((slider.max - slider.min) / slider.step).roundToInt() - 1).coerceAtLeast(0),
                )
            }
            kind.result(values.toList()).forEach { line ->
                Text(line, style = MaterialTheme.typography.bodyLarge)
            }
        }
    }
}

private fun snap(raw: Double, slider: CalcSlider): Double {
    val steps = ((raw - slider.min) / slider.step).roundToInt()
    return (slider.min + steps * slider.step).coerceIn(slider.min, slider.max)
}

private fun decimals(step: Double): Int = when {
    step >= 1.0 -> 0
    step >= 0.1 -> 1
    else -> 3
}
