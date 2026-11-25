package com.automatika.autopilot.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class MenuGroupingTest {
    @Test
    fun everyRootItemIsInExactlyOneSection() {
        HmiMenu.clearCache()
        val file = listOf(
            File("src/main/assets/hmi_menu_tree.json"),
            File("android/app/src/main/assets/hmi_menu_tree.json"),
        ).first { it.exists() }
        val root = HmiMenu.loadFromJson(file.readText(Charsets.UTF_8)) + MenuItem(
            id = "app.page", icon = "⚙", title = "Приложение", type = MenuType.SCREEN, colorCategory = 10
        )
        val groups = groupRootMenu(root)
        assertFalse(groups.any { it.first == "Прочее" })
        assertEquals(root.size, groups.sumOf { it.second.size })
    }

    @Test
    fun hmiSectionsPresent() {
        HmiMenu.clearCache()
        val file = listOf(
            File("src/main/assets/hmi_menu_tree.json"),
            File("android/app/src/main/assets/hmi_menu_tree.json"),
        ).first { it.exists() }
        val root = HmiMenu.loadFromJson(file.readText(Charsets.UTF_8))
        val titles = groupRootMenu(root).map { it.first }
        assertTrue(titles.contains("Режимы"))
        assertTrue(titles.contains("Подготовка"))
        assertTrue(titles.contains("Диагностика"))
        assertTrue(titles.contains("Система"))
        assertTrue(titles.contains("Справка"))
    }
}
