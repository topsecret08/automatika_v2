package com.automatika.autopilot.ui

import androidx.compose.foundation.Image
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
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.automatika.autopilot.R
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
    /** Только просмотр JSON-документа контроллера, без сохранения. */
    data class View(val path: String, val title: String) : MenuOutcome()
    data class Calc(val kind: CalcKind) : MenuOutcome()
    object Graph : MenuOutcome()
    object History : MenuOutcome()
    object AiStatus : MenuOutcome()
    data class Maintenance(val kind: MaintKind) : MenuOutcome()
    data class Recipes(val category: String, val title: String) : MenuOutcome()
    data class Help(val articleId: String, val title: String) : MenuOutcome()
    data class Diag(val kind: DiagKind) : MenuOutcome()
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

    private val calcByCallback = mapOf(
        "cb_calc_dil" to CalcKind.DILUTION,
        "cb_calc_sug" to CalcKind.SUGAR,
        "cb_calc_grn" to CalcKind.GRAIN,
        "cb_calc_out" to CalcKind.YIELD,
        "cb_calc_abv" to CalcKind.ABV,
        "cb_calc_tmp" to CalcKind.TEMP,
        "cb_alc_temp" to CalcKind.TEMP,
        "cb_calc_prs" to CalcKind.PRESSURE,
        "cb_calc_gab" to CalcKind.GABRIEL,
    )

    private val maintByCallback = mapOf(
        "cb_ai_reset" to MaintKind.RESET_AI,
        "cb_al_reset" to MaintKind.RESET_AUTOLEARN,
        "cb_tm_reset" to MaintKind.RESET_TINYML,
        "cb_db_backup" to MaintKind.DB_BACKUP,
        "cb_db_vacuum" to MaintKind.DB_VACUUM,
        "cb_db_stats" to MaintKind.DB_STATS,
        "cb_db_export" to MaintKind.DB_EXPORT,
        "cb_usb_status" to MaintKind.USB,
        "cb_scale_calib" to MaintKind.SCALE,
        "cb_senscal_open" to MaintKind.SENSOR_CALIB,
        "cb_valve_calib_menu" to MaintKind.VALVE_STATE,
        "cb_valve_calib_show" to MaintKind.VALVE_STATE,
    )

    private val recipeCategories = mapOf(
        "cb_recipes_beer" to ("beer" to "Пивные рецепты"),
        "cb_recipes_spirit" to ("spirit" to "Рецепты спиртов"),
        "cb_recipes_mash" to ("mash" to "Затирки"),
    )

    /** Пункты справки: какая статья открывается. Названия берутся из пункта меню. */
    private val helpArticles = mapOf(
        "cb_help_basics" to "01_intro",
        "cb_help_work" to "04_distill",
        "cb_help_system" to "59_ai",
        "cb_help_hw" to "19_ds18b20",
        "cb_help_schemes" to "81_schemes",
        "cb_help_safety" to "03_safety",
        "cb_help_recipes" to "74_recipes",
        "cb_help_beer" to "06_beer",
        "cb_help_vacuum" to "09_vacuum",
        "cb_help_double" to "10_double",
        "cb_help_gabriel" to "11_gabriel",
        "cb_help_nbk" to "08_nbk",
        "cb_help_compare" to "04_distill",
        "cb_help_history" to "84_history",
        "cb_help_maint" to "87_maintenance",
        "cb_help_whatsnew" to "90_whatsnew",
    )

    /** Экраны только для чтения: JSON, который уже отдаёт контроллер. */
    private val viewPaths = mapOf(
        "cb_diag_i2c" to "/api/hardware",
        "cb_diag_mon" to "/api/state",
        "cb_diag_perf" to "/api/health",
        "cb_usb_config" to "/api/usb/status",
        "cb_fota_status" to "/api/ota",
        "cb_alc_show" to "/api/alcohol",
        "cb_settings_pid" to "/api/autotune",
    )

    /** Экраны ИИ, которые показывает общая карточка статуса ИИ. */
    private val aiScreens = setOf(
        "cb_ai_predict", "cb_ai_action", "cb_ai_anomaly", "cb_ai_warnings_open", "cb_ai_cache_cfg",
    )

    private val diagKinds = mapOf(
        "cb_diag_over" to DiagKind.OVERVIEW,
        "cb_diag_ds" to DiagKind.SENSORS,
        "cb_wd_status" to DiagKind.WATCHDOG,
    )

    fun resolve(item: MenuItem): MenuOutcome {
        if (item.type == MenuType.SUBMENU) return MenuOutcome.Open
        startByCallback[item.action]?.let { return MenuOutcome.Start(it) }
        calcByCallback[item.action]?.let { return MenuOutcome.Calc(it) }
        if (item.action == "cb_graph") return MenuOutcome.Graph
        if (item.action == "cb_graph_history") return MenuOutcome.History
        if (item.action == "cb_ai_status" || item.action in aiScreens) return MenuOutcome.AiStatus
        viewPaths[item.action]?.let { return MenuOutcome.View(it, item.title) }
        maintByCallback[item.action]?.let { return MenuOutcome.Maintenance(it) }
        recipeCategories[item.action]?.let { (category, title) -> return MenuOutcome.Recipes(category, title) }
        helpArticles[item.action]?.let { return MenuOutcome.Help(it, item.title) }
        diagKinds[item.action]?.let { return MenuOutcome.Diag(it) }
        return when (item.action) {
            "cb_safety_reset" -> MenuOutcome.ResetAlarm
            "cb_settings_hw" -> MenuOutcome.Editor("/api/hardware", item.title)
            "cb_settings_gen" -> MenuOutcome.Editor("/api/config", item.title)
            else -> MenuOutcome.OnDeviceOnly
        }
    }
}

/** Пиктограммы режимов из docs/design/mode-icons. Для остальных пунктов возвращает null. */
fun modeIconRes(action: String): Int? = when (action) {
    "cb_start_distill" -> R.drawable.ic_mode_distill
    "cb_start_rectify" -> R.drawable.ic_mode_rectify
    "cb_start_beer" -> R.drawable.ic_mode_beer
    "cb_start_pvk" -> R.drawable.ic_mode_pvk
    "cb_start_nbk" -> R.drawable.ic_mode_nbk
    "cb_start_vacuum" -> R.drawable.ic_mode_vacuum
    else -> null
}

/**
 * Разделы главного меню приложения. Ключи — id пунктов из menu_tree.c.
 * Состав дерева не меняется: группировка только для экрана приложения.
 */
private val MENU_SECTIONS: List<Pair<String, List<String>>> = listOf(
    "Режимы" to listOf("menu.m", "menu.mon", "menu.f", "menu.rec", "menu.c", "menu.ab", "menu.tp"),
    "Настройка" to listOf("app.page", "menu.s", "menu.v", "menu.ui", "menu.r", "menu.sc", "menu.al", "menu.tm", "menu.ex"),
    "Диагностика" to listOf("menu.d", "menu.wd", "menu.db", "menu.usb", "menu.fo"),
    "Справка" to listOf("menu.h", "menu.l", "menu.dm", "menu.ai", "menu.qr"),
)

/** Пункты главного меню по разделам. Пункт вне разделов попадает в «Прочее». */
fun groupRootMenu(items: List<MenuItem>): List<Pair<String, List<MenuItem>>> {
    val byId = items.associateBy { it.id }
    val used = mutableSetOf<String>()
    val groups = MENU_SECTIONS.map { (title, ids) ->
        used += ids
        title to ids.mapNotNull { byId[it] }
    }.toMutableList()
    val rest = items.filter { it.id !in used }
    if (rest.isNotEmpty()) groups += "Прочее" to rest
    return groups.filter { it.second.isNotEmpty() }
}

/** Плитки меню нейтральные: цвет зарезервирован за состояниями, а не за категориями. */
private val CategoryColors = List(11) { Color(0xFF35424A) }

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
                val modeIcon = modeIconRes(item.action)
                if (modeIcon != null) {
                    Image(
                        painter = painterResource(modeIcon),
                        contentDescription = null,
                        modifier = Modifier.size(40.dp),
                    )
                } else {
                    Text(item.icon, fontSize = 20.sp, modifier = Modifier.size(32.dp, 24.dp))
                }
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
