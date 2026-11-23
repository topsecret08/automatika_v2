package com.automatika.autopilot.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.automatika.autopilot.data.ControllerState
import java.util.Locale

/** Сколько точек хранится: при опросе раз в 2 с это около 10 минут. */
const val GRAPH_SAMPLES = 300

/** Диапазон оси по значениям с запасом 2 °C. Null, если значений нет. */
fun axisRange(values: List<Double>): Pair<Double, Double>? {
    val lo = values.minOrNull() ?: return null
    val hi = values.maxOrNull() ?: return null
    return (lo - 2.0) to (if (hi - lo < 1.0) lo + 3.0 else hi + 2.0)
}

/** График температур по данным опроса. Историю контроллер не отдаёт, поэтому ведём её в приложении. */
@Composable
fun GraphCard(state: ControllerState?) {
    val samples = remember { mutableStateListOf<ControllerState>() }
    LaunchedEffect(state) {
        if (state != null) {
            samples.add(state)
            while (samples.size > GRAPH_SAMPLES) samples.removeAt(0)
        }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Температуры, последние ~10 мин", style = MaterialTheme.typography.titleMedium)
            Text(
                "Куб ${formatTemp(state?.cube)} °C · Колонна ${formatTemp(state?.column)} °C",
                style = MaterialTheme.typography.bodyMedium,
            )
            Canvas(Modifier.fillMaxWidth().height(200.dp)) {
                val all = samples.flatMap { listOf(it.cube, it.column) }.filter { !it.isNaN() }
                val range = axisRange(all) ?: return@Canvas
                val (lo, hi) = range
                val n = samples.size
                if (n < 2) return@Canvas
                val dx = size.width / (GRAPH_SAMPLES - 1).toFloat()
                fun yOf(v: Double): Float = (size.height * (1.0 - (v - lo) / (hi - lo))).toFloat()
                fun series(pick: (ControllerState) -> Double, color: Color) {
                    var prev: Offset? = null
                    samples.forEachIndexed { i, s ->
                        val v = pick(s)
                        if (v.isNaN()) {
                            prev = null
                        } else {
                            val p = Offset(dx * (GRAPH_SAMPLES - n + i), yOf(v))
                            prev?.let { drawLine(color, it, p, strokeWidth = 3f) }
                            prev = p
                        }
                    }
                }
                series({ it.cube }, Color(0xFFF2F5F7))
                series({ it.column }, Color(0xFF2F8CFF))
            }
        }
    }
}

private fun formatTemp(value: Double?): String =
    if (value == null || value.isNaN()) "—" else String.format(Locale.US, "%.1f", value)
