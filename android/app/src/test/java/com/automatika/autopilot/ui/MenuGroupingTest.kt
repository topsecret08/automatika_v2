package com.automatika.autopilot.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class MenuGroupingTest {
    @Test
    fun everyRootItemIsInExactlyOneSection() {
        val groups = groupRootMenu(ROOT_MENU)
        assertFalse(groups.any { it.first == "Прочее" })
        assertEquals(ROOT_MENU.size, groups.sumOf { it.second.size })
    }

    @Test
    fun sectionsKeepTheRequestedOrder() {
        val titles = groupRootMenu(ROOT_MENU).map { it.first }
        assertEquals(listOf("Процесс", "Контроль", "Оборудование", "Настройки", "Данные", "Система", "Справка"), titles)
    }
}
