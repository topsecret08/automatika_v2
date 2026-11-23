package com.automatika.autopilot.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Палитра по правилам UI, та же, что в PWA (littlefs/css/tokens.css) и на LCD.
// Синий — действия, янтарный — предупреждение и уставки, красный — авария,
// зелёный — норма. Остальное нейтрально.
private val AutomatikaColors = darkColorScheme(
    primary = Color(0xFF2F8CFF),
    onPrimary = Color(0xFFF2F5F7),
    primaryContainer = Color(0xFF1A242B),
    onPrimaryContainer = Color(0xFFF2F5F7),
    secondary = Color(0xFFAAB5BC),
    tertiary = Color(0xFFF5A524),
    tertiaryContainer = Color(0xFF3D2C10),
    onTertiaryContainer = Color(0xFFF2F5F7),
    error = Color(0xFFD93B3B),
    background = Color(0xFF121820),
    onBackground = Color(0xFFF2F5F7),
    surface = Color(0xFF1A242B),
    onSurface = Color(0xFFF2F5F7),
    surfaceVariant = Color(0xFF1A242B),
    onSurfaceVariant = Color(0xFFAAB5BC),
    outline = Color(0xFF35424A),
)

@Composable
fun AutomatikaTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = AutomatikaColors, content = content)
}
