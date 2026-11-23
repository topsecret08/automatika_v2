package com.automatika.autopilot

import com.automatika.autopilot.data.ProcessMode
import com.automatika.autopilot.ui.FieldNames
import com.automatika.autopilot.ui.DiagKind
import com.automatika.autopilot.ui.MaintKind
import com.automatika.autopilot.ui.MenuActions
import com.automatika.autopilot.ui.MenuItem
import com.automatika.autopilot.ui.MenuOutcome
import com.automatika.autopilot.ui.MenuRules
import com.automatika.autopilot.ui.MenuType
import com.automatika.autopilot.ui.ROOT_MENU
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MenuTreeTest {

    private fun root(id: String): MenuItem = ROOT_MENU.first { it.id == id }

    private fun child(parent: String, id: String): MenuItem =
        root(parent).children.first { it.id == id }

    @Test
    fun rootMatchesDeviceMenu() {
        assertEquals(25, ROOT_MENU.size)
        assertEquals("Режимы работы", ROOT_MENU.first().title)
        assertTrue(ROOT_MENU.all { it.type == MenuType.SUBMENU })
    }

    @Test
    fun startItemsResolveToProcessModes() {
        assertEquals(MenuOutcome.Start(ProcessMode.DISTILL), MenuActions.resolve(child("menu.m", "m.distill")))
        assertEquals(MenuOutcome.Start(ProcessMode.BEER), MenuActions.resolve(child("menu.m", "m.beer")))
    }

    @Test
    fun safetyResetRequiresConfirmation() {
        assertEquals(MenuOutcome.ResetAlarm, MenuActions.resolve(child("menu.d", "d.safe")))
    }

    @Test
    fun settingsEditorsOpenDeviceConfigAndHardware() {
        assertEquals(
            MenuOutcome.Editor("/api/hardware", child("menu.s", "s.hw").title),
            MenuActions.resolve(child("menu.s", "s.hw")),
        )
        assertEquals(
            MenuOutcome.Editor("/api/config", child("menu.s", "s.gen").title),
            MenuActions.resolve(child("menu.s", "s.gen")),
        )
    }

    @Test
    fun unsupportedItemsAreMarkedAsDeviceOnly() {
        assertEquals(MenuOutcome.OnDeviceOnly, MenuActions.resolve(child("menu.mon", "mon.stats")))
    }

    @Test
    fun historyAndAiStatusOpenInApp() {
        assertEquals(MenuOutcome.History, MenuActions.resolve(child("menu.mon", "mon.hist")))
        assertEquals(MenuOutcome.AiStatus, MenuActions.resolve(child("menu.ai", "ai.status")))
    }

    @Test
    fun maintenanceActionsOpenTheirScreens() {
        fun action(callback: String) = MenuItem(id = "x", icon = "", title = "x", type = MenuType.ACTION, action = callback)
        assertEquals(MenuOutcome.Maintenance(MaintKind.RESET_AI), MenuActions.resolve(action("cb_ai_reset")))
        assertEquals(MenuOutcome.Maintenance(MaintKind.DB_BACKUP), MenuActions.resolve(action("cb_db_backup")))
        assertEquals(MenuOutcome.Maintenance(MaintKind.SCALE), MenuActions.resolve(action("cb_scale_calib")))
        assertEquals(MenuOutcome.Maintenance(MaintKind.DB_EXPORT), MenuActions.resolve(action("cb_db_export")))
        assertEquals(MenuOutcome.Maintenance(MaintKind.VALVE_STATE), MenuActions.resolve(action("cb_valve_calib_show")))
    }

    @Test
    fun recipeCategoriesOpenInAppOthersStayOnDevice() {
        fun action(callback: String) = MenuItem(id = "x", icon = "", title = "x", type = MenuType.ACTION, action = callback)
        assertEquals(MenuOutcome.Recipes("beer", "Пивные рецепты"), MenuActions.resolve(action("cb_recipes_beer")))
        assertEquals(MenuOutcome.Recipes("spirit", "Рецепты спиртов"), MenuActions.resolve(action("cb_recipes_spirit")))
        assertEquals(MenuOutcome.OnDeviceOnly, MenuActions.resolve(action("cb_recipes_mine")))
    }

    @Test
    fun helpAndDiagnosticsOpenInApp() {
        fun action(callback: String, title: String = "x") =
            MenuItem(id = "x", icon = "", title = title, type = MenuType.ACTION, action = callback)
        assertEquals(MenuOutcome.Help("01_intro", "Основы"), MenuActions.resolve(action("cb_help_basics", "Основы")))
        assertEquals(MenuOutcome.Diag(DiagKind.SENSORS), MenuActions.resolve(action("cb_diag_ds")))
        assertEquals(MenuOutcome.Diag(DiagKind.WATCHDOG), MenuActions.resolve(action("cb_wd_status")))
        assertEquals(MenuOutcome.OnDeviceOnly, MenuActions.resolve(action("cb_diag_test")))
    }

    @Test
    fun realtimeGraphOpensInApp() {
        assertEquals(MenuOutcome.Graph, MenuActions.resolve(child("menu.mon", "mon.graph")))
    }

    @Test
    fun roleRulesMatchDevice() {
        val engineerOnly = MenuItem(id = "x", icon = "", title = "x", type = MenuType.ACTION, requiredRole = 2)
        assertFalse(MenuRules.isAvailable(engineerOnly, 1))
        assertTrue(MenuRules.isAvailable(engineerOnly, 2))
        assertTrue(MenuRules.isAvailable(engineerOnly, 3))
    }

    @Test
    fun fieldNamesAreHumanReadable() {
        assertEquals("Гистерезис, °C", FieldNames.label("hys"))
        assertEquals("неизвестный_ключ (ключ API)", FieldNames.label("неизвестный_ключ"))
        assertTrue(FieldNames.isReadOnly("schema"))
        assertFalse(FieldNames.isReadOnly("hys"))
    }
}
