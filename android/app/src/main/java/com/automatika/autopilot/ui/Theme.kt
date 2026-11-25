package com.automatika.autopilot.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// HMI-стиль: тёмный фон, стеклянные карточки, циановый акцент.
private val AutomatikaColors = darkColorScheme(
    primary = Color(0xFF4CC9F0),
    onPrimary = Color(0xFF071018),
    primaryContainer = Color(0xFF16324A),
    onPrimaryContainer = Color(0xFFE6F7FF),
    secondary = Color(0xFF8FB3C9),
    tertiary = Color(0xFFF5A524),
    tertiaryContainer = Color(0xFF3D2C10),
    onTertiaryContainer = Color(0xFFF2F5F7),
    error = Color(0xFFFF5C5C),
    background = Color(0xFF0B141C),
    onBackground = Color(0xFFEAF2F7),
    surface = Color(0xFF142231),
    onSurface = Color(0xFFEAF2F7),
    surfaceVariant = Color(0xFF1B3042),
    onSurfaceVariant = Color(0xFF9FB6C6),
    outline = Color(0xFF2C4A61),
)

@Composable
fun AutomatikaTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = AutomatikaColors, content = content)
}
