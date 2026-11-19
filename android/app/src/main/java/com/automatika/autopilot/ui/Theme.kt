package com.automatika.autopilot.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Палитра совпадает с PWA (littlefs/css/tokens.css): тёмный navy, синий акцент.
private val AutomatikaColors = darkColorScheme(
    primary = Color(0xFF2F8CFF),
    onPrimary = Color.White,
    primaryContainer = Color(0xFF12324F),
    onPrimaryContainer = Color.White,
    secondary = Color(0xFF5E5CE6),
    tertiary = Color(0xFFF5A524),
    tertiaryContainer = Color(0xFF3D2C10),
    onTertiaryContainer = Color.White,
    error = Color(0xFFD93B3B),
    background = Color(0xFF070B11),
    onBackground = Color.White,
    surface = Color(0xFF111A26),
    onSurface = Color.White,
    surfaceVariant = Color(0xFF1D2735),
    onSurfaceVariant = Color(0xFFC8D6E8),
    outline = Color(0xFF3A4D66),
)

@Composable
fun AutomatikaTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = AutomatikaColors, content = content)
}
