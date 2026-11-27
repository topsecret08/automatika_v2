package com.automatika.autopilot.ui

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/**
 * Меню HMI (firmware_hmi/ui_tree/menu_tree.json), встроенное в APK как asset.
 * Гарантирует наличие всех режимов, настроек, справки и ветвей дерева HMI.
 */
object HmiMenu {
    data class HmiScreen(
        val id: String,
        val title: String,
        val parent: String?,
        val children: List<String>,
        val manual: String?,
        val kind: String,
    )

    private var screens: Map<String, HmiScreen>? = null
    private var rootChildren: List<MenuItem>? = null

    fun load(context: Context): List<MenuItem> {
        rootChildren?.let { return it }
        val text = context.assets.open("hmi_menu_tree.json").bufferedReader().use { it.readText() }
        val items = loadFromJson(text)
        val augmented = augmentWithLocalContent(context, items)
        rootChildren = augmented
        return augmented
    }

    fun loadFromJson(text: String): List<MenuItem> {
        val map = parseJson(text)
        screens = map
        val main = map["main"] ?: return emptyList()
        val childIds = main.children.toMutableList()
        for ((id, s) in map) {
            if (s.parent == "main" && id !in childIds) childIds.add(id)
        }
        val items = childIds.mapNotNull { map[it]?.toMenuItem(map) }
        rootChildren = items
        return items
    }

    private fun augmentWithLocalContent(context: Context, items: List<MenuItem>): List<MenuItem> {
        return items.map { item ->
            when (item.id) {
                "help" -> item.copy(children = buildHelpChildren(context, item.children))
                "preparation" -> item.copy(children = ensureRecipesNode(item.children))
                else -> item
            }
        }
    }

    private fun buildHelpChildren(context: Context, existing: List<MenuItem>): List<MenuItem> {
        val existingIds = existing.map { it.id }.toMutableSet()
        val out = existing.toMutableList()
        val files = runCatching { context.assets.list("static/help")?.toList().orEmpty() }.getOrDefault(emptyList())
        val articles = files.filter { it.endsWith(".md") }.sorted()
        for (f in articles) {
            val articleId = f.removeSuffix(".md")
            val nodeId = "help_article_$articleId"
            if (nodeId in existingIds) continue
            val title = runCatching {
                context.assets.open("static/help/$f").bufferedReader().use { br ->
                    br.lineSequence().firstOrNull { it.trim().startsWith("#") }
                        ?.trim()?.trimStart('#', ' ')?.ifBlank { articleId } ?: articleId
                }
            }.getOrDefault(articleId)
            existingIds += nodeId
            out += MenuItem(
                id = nodeId,
                icon = "📄",
                title = title,
                type = MenuType.SCREEN,
                action = "cb_help_article",
                helpId = articleId,
                colorCategory = 9,
            )
        }
        return out
    }

    private fun ensureRecipesNode(children: List<MenuItem>): List<MenuItem> {
        if (children.any { it.id == "recipes_all" }) return children
        val recipes = MenuItem(
            id = "recipes_all",
            icon = "📚",
            title = "Рецепты",
            type = MenuType.SUBMENU,
            colorCategory = 0,
            children = listOf(
                MenuItem(id = "recipes_beer", icon = "🍺", title = "Рецепты пива", type = MenuType.SCREEN, action = "cb_recipes_beer", colorCategory = 0),
                MenuItem(id = "recipes_spirit", icon = "🥃", title = "Рецепты дистиллятов", type = MenuType.SCREEN, action = "cb_recipes_spirit", colorCategory = 0),
                MenuItem(id = "recipes_mash", icon = "🌾", title = "Затирание", type = MenuType.SCREEN, action = "cb_recipes_mash", colorCategory = 0),
            ),
        )
        return children + recipes
    }

    fun clearCache() {
        screens = null
        rootChildren = null
    }

    fun find(id: String): HmiScreen? = screens?.get(id)

    fun allScreens(): Collection<HmiScreen> = screens?.values ?: emptyList()

    fun parseJson(text: String): Map<String, HmiScreen> {
        val root = JSONObject(text)
        val arr = root.getJSONArray("screens")
        val map = LinkedHashMap<String, HmiScreen>(arr.length())
        for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            val children = o.optJSONArray("children")?.toStringList() ?: emptyList()
            val s = HmiScreen(
                id = o.getString("id"),
                title = o.getString("title"),
                parent = if (o.isNull("parent")) null else o.optString("parent"),
                children = children,
                manual = if (o.isNull("manual")) null else o.optString("manual"),
                kind = o.optString("kind", "leaf"),
            )
            map[s.id] = s
        }
        return map
    }

    private fun JSONArray.toStringList(): List<String> {
        val out = ArrayList<String>(length())
        for (i in 0 until length()) out.add(getString(i))
        return out
    }

    private fun HmiScreen.toMenuItem(map: Map<String, HmiScreen>): MenuItem {
        val childItems = children.mapNotNull { map[it]?.toMenuItem(map) }
        val type = when {
            childItems.isNotEmpty() -> MenuType.SUBMENU
            else -> hmiLeafType(id)
        }
        val action = hmiAction(id)
        return MenuItem(
            id = id,
            icon = hmiIcon(id),
            title = title,
            type = type,
            children = childItems,
            action = action,
            requiredRole = hmiRole(id),
            helpId = hmiHelpId(id),
            colorCategory = hmiCategory(id),
        )
    }

    private fun hmiLeafType(id: String): MenuType = when {
        id.startsWith("calc_") -> MenuType.SCREEN
        id.startsWith("help_") -> MenuType.SCREEN
        id.startsWith("diag_test") || id.startsWith("prep_check") -> MenuType.SCREEN
        else -> MenuType.SCREEN
    }

    private fun hmiAction(id: String): String = when (id) {
        "mode_distillation", "work_distillation", "dash_distillation" -> "cb_start_distill"
        "mode_rectification", "work_rectification", "dash_rectification" -> "cb_start_rectify"
        "mode_beer", "work_beer", "dash_beer" -> "cb_start_beer"
        "mode_pvk", "work_pvk", "dash_pvk" -> "cb_start_pvk"
        "mode_nbk", "work_nbk", "dash_nbk" -> "cb_start_nbk"
        "mode_vacuum", "work_vacuum", "dash_vacuum" -> "cb_start_vacuum"
        "mode_double", "work_double", "dash_double" -> "cb_start_double"
        "mode_fermentation", "work_fermentation", "dash_fermentation" -> "cb_start_ferment"
        "calc_strength" -> "cb_calc_abv"
        "calc_temp" -> "cb_calc_tmp"
        "calc_flow" -> "cb_calc_out"
        "calc_units" -> "cb_calc_dil"
        "help_current", "help_glossary", "help_about" -> "cb_help_basics"
        "help_safety" -> "cb_help_safety"
        "diag_journal_events", "diag_journal_alarms" -> "cb_graph_history"
        "diag_journal_can", "diag_net_can", "diag_net_wifi", "diag_net_api", "diag_net_ping" -> "cb_diag_over"
        "diag_ai_status", "diag_ai_predict", "diag_ai_log", "diag_ai_update" -> "cb_ai_status"
        "sys_api_endpoints", "sys_api_token", "sys_api_rate", "sys_api_clients" -> "cb_settings_gen"
        "sys_update_check", "sys_update_hmi", "sys_update_ctrl", "sys_update_rollback" -> "cb_fota_status"
        "sys_io_backup", "sys_io_restore", "sys_io_factory" -> "cb_settings_exp"
        "prep_prof_list", "prep_prof_new", "prep_prof_copy", "prep_prof_delete" -> "cb_recipes_beer"
        "prep_check_sensors", "prep_check_actuators", "prep_check_links" -> "cb_diag_over"
        "diag_test_temp", "diag_test_adc", "diag_test_gpio", "diag_test_relay" -> "cb_diag_ds"
        "diag_act_manual", "diag_act_relay", "diag_act_pwm" -> "cb_diag_over"
        else -> ""
    }

    private fun hmiHelpId(id: String): String = when {
        id.startsWith("mode_distillation") || id.contains("distillation") -> "04_distill"
        id.startsWith("mode_rectification") || id.contains("rectification") -> "05_rectify"
        id.startsWith("mode_beer") || id.contains("_beer") -> "06_beer"
        id.startsWith("mode_pvk") || id.contains("_pvk") -> "07_pvk"
        id.startsWith("mode_nbk") || id.contains("_nbk") -> "08_nbk"
        id.startsWith("mode_vacuum") || id.contains("_vacuum") -> "09_vacuum"
        id.startsWith("mode_double") || id.contains("_double") -> "10_double"
        id.startsWith("mode_fermentation") || id.contains("fermentation") -> "17_fermentation"
        id.startsWith("help_safety") -> "03_safety"
        id.startsWith("help_") -> "01_intro"
        id.startsWith("calc_") -> "75_calcs"
        else -> ""
    }

    private fun hmiCategory(id: String): Int = when {
        id == "modes" || id.startsWith("mode_") || id.startsWith("settings_") || id.startsWith("work_") ||
            id.startsWith("params_") || id.startsWith("recipes_") || id.startsWith("equipment_") ||
            id.startsWith("limits_") || id.startsWith("recipe_") || id.startsWith("dash_") ||
            id.startsWith("charts_") || id.startsWith("journal_") || id.startsWith("alarms_") || id.startsWith("report_") -> 0
        id == "preparation" || id.startsWith("prep_") -> 5
        id == "diagnostics" || id.startsWith("diag_") -> 6
        id == "system" || id.startsWith("sys_") -> 6
        id == "help" || id.startsWith("help_") -> 9
        id == "calculators" || id.startsWith("calc_") -> 3
        else -> 0
    }

    private fun hmiRole(id: String): Int = when {
        id.startsWith("sys_") || id.startsWith("prep_cal") || id.startsWith("diag_act_manual") -> 2
        else -> 0
    }

    private fun hmiIcon(id: String): String = when (id) {
        "modes" -> "⚙"
        "mode_distillation" -> "🥃"
        "mode_rectification" -> "🔬"
        "mode_beer" -> "🍺"
        "mode_pvk" -> "💧"
        "mode_nbk" -> "🔄"
        "mode_vacuum" -> "🫙"
        "mode_double" -> "♻"
        "mode_fermentation" -> "🧪"
        "preparation" -> "🧰"
        "diagnostics" -> "🩺"
        "system" -> "🖥"
        "help" -> "❓"
        "calculators" -> "🧮"
        else -> when {
            id.startsWith("settings_") -> "⚙"
            id.startsWith("work_") || id.startsWith("dash_") -> "▶"
            id.startsWith("charts_") -> "📈"
            id.startsWith("journal_") || id.startsWith("diag_journal") -> "📋"
            id.startsWith("alarms_") -> "🚨"
            id.startsWith("report_") -> "📄"
            id.startsWith("recipe") || id.startsWith("prep_prof") -> "📚"
            id.startsWith("params_") || id.startsWith("limits_") -> "🎚"
            id.startsWith("equipment_") || id.startsWith("prep_eq") -> "🔌"
            id.startsWith("prep_cal") -> "📏"
            id.startsWith("prep_check") -> "✅"
            id.startsWith("prep_import") || id.startsWith("sys_io") -> "📦"
            id.startsWith("diag_test") -> "🧪"
            id.startsWith("diag_act") -> "🦾"
            id.startsWith("diag_ai") -> "🤖"
            id.startsWith("diag_net") -> "🌐"
            id.startsWith("sys_update") -> "⬆"
            id.startsWith("sys_api") -> "🔑"
            id.startsWith("sys_lang") -> "🌍"
            id.startsWith("sys_roles") -> "👤"
            id.startsWith("sys_ui") -> "🎨"
            id.startsWith("sys_srv") -> "🛠"
            else -> "•"
        }
    }
}
