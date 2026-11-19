package com.automatika.autopilot.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.automatika.autopilot.data.ProcessMode

/** Роли как на дисплее контроллера (main/core/roles.h). */
object Roles {
    const val GUEST = 0
    const val OPERATOR = 1
    const val ENGINEER = 2
    const val ADMIN = 3

    fun name(role: Int): String = when (role) {
        GUEST -> "Гость"
        OPERATOR -> "Оператор"
        ENGINEER -> "Инженер"
        ADMIN -> "Администратор"
        else -> "—"
    }
}

/** Правило из menu_engine.c: пункт доступен, если роль не ниже требуемой. */
object MenuRules {
    fun isAvailable(item: MenuItem, role: Int): Boolean = role >= item.requiredRole
}

/** Что делает пункт меню в приложении. Остальные пункты открываются только на дисплее. */
sealed class MenuOutcome {
    object Open : MenuOutcome()
    data class Start(val mode: ProcessMode) : MenuOutcome()
    object ResetAlarm : MenuOutcome()
    data class Editor(val path: String, val title: String) : MenuOutcome()
    object OnDeviceOnly : MenuOutcome()
}

object MenuActions {
    private val startByCallback = mapOf(
        "cb_start_distill" to ProcessMode.DISTILL,
        "cb_start_rectify" to ProcessMode.RECTIFY,
        "cb_start_beer" to ProcessMode.BEER,
        "cb_start_pvk" to ProcessMode.PVK,
        "cb_start_nbk" to ProcessMode.NBK,
        "cb_start_vacuum" to ProcessMode.VACUUM,
        "cb_start_double" to ProcessMode.DOUBLE,
        "cb_start_ferment" to ProcessMode.FERMENT,
    )

    fun resolve(item: MenuItem): MenuOutcome {
        if (item.type == MenuType.SUBMENU) return MenuOutcome.Open
        startByCallback[item.action]?.let { return MenuOutcome.Start(it) }
        return when (item.action) {
            "cb_safety_reset" -> MenuOutcome.ResetAlarm
            "cb_settings_hw" -> MenuOutcome.Editor("/api/hardware", item.title)
            "cb_settings_gen" -> MenuOutcome.Editor("/api/config", item.title)
            else -> MenuOutcome.OnDeviceOnly
        }
    }
}

/** Цвета категорий меню, как colorCategory в menu_tree.c (0..10). */
private val CategoryColors = listOf(
    Color(0xFF1E88E5), Color(0xFF43A047), Color(0xFFFB8C00), Color(0xFF8E24AA),
    Color(0xFFD81B60), Color(0xFF00897B), Color(0xFF6D4C41), Color(0xFF3949AB),
    Color(0xFFC0CA33), Color(0xFF546E7A), Color(0xFFE53935),
)

fun categoryColor(category: Int): Color =
    CategoryColors[category.coerceIn(0, CategoryColors.lastIndex)]

/** Плитки главного меню: по три в ряд, как на дисплее. */
@Composable
fun MenuTiles(items: List<MenuItem>, role: Int, onClick: (MenuItem) -> Unit) {
    items.chunked(3).forEach { row ->
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            row.forEach { item ->
                val available = MenuRules.isAvailable(item, role)
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .height(104.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(categoryColor(item.colorCategory).copy(alpha = if (available) 1f else 0.35f))
                        .clickable { onClick(item) }
                        .padding(8.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                ) {
                    Text(item.icon, fontSize = 26.sp)
                    Text(
                        item.title,
                        color = Color.White,
                        fontWeight = FontWeight.Medium,
                        textAlign = TextAlign.Center,
                        style = MaterialTheme.typography.bodySmall,
                    )
                    if (!available) {
                        Text("нужна роль: ${Roles.name(item.requiredRole)}", color = Color.White, style = MaterialTheme.typography.labelSmall)
                    }
                }
            }
            repeat(3 - row.size) { Spacer(Modifier.weight(1f)) }
        }
    }
}

/** Список пунктов подменю, как на дисплее. */
@Composable
fun MenuList(items: List<MenuItem>, role: Int, onClick: (MenuItem) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        items.forEach { item ->
            val available = MenuRules.isAvailable(item, role)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(10.dp))
                    .background(categoryColor(item.colorCategory).copy(alpha = if (available) 0.18f else 0.06f))
                    .clickable { onClick(item) }
                    .padding(12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(item.icon, fontSize = 20.sp, modifier = Modifier.size(32.dp, 24.dp))
                Spacer(Modifier.size(8.dp))
                Text(item.title, Modifier.weight(1f), style = MaterialTheme.typography.bodyLarge)
                val hint = when {
                    !available -> "роль: ${Roles.name(item.requiredRole)}"
                    item.type == MenuType.SUBMENU -> "›"
                    else -> ""
                }
                if (hint.isNotEmpty()) {
                    Text(hint, style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }
}
