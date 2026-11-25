package com.automatika.autopilot

import com.automatika.autopilot.data.ProcessMode
import com.automatika.autopilot.ui.HmiMenu
import com.automatika.autopilot.ui.MenuActions
import com.automatika.autopilot.ui.MenuItem
import com.automatika.autopilot.ui.MenuOutcome
import com.automatika.autopilot.ui.MenuType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.io.File

class MenuTreeTest {

    private lateinit var root: List<MenuItem>

    @Before
    fun load() {
        HmiMenu.clearCache()
        val candidates = listOf(
            File("src/main/assets/hmi_menu_tree.json"),
            File("android/app/src/main/assets/hmi_menu_tree.json"),
            File("../android/app/src/main/assets/hmi_menu_tree.json"),
        )
        val file = candidates.firstOrNull { it.exists() }
            ?: error("hmi_menu_tree.json not found; tried: ${candidates.map { it.absolutePath }}")
        root = HmiMenu.loadFromJson(file.readText(Charsets.UTF_8))
    }

    private fun allItems(items: List<MenuItem> = root): List<MenuItem> =
        items.flatMap { listOf(it) + allItems(it.children) }

    private fun byId(id: String): MenuItem? = allItems().firstOrNull { it.id == id }

    @Test
    fun rootHasHmiSections() {
        val ids = root.map { it.id }
        assertTrue(ids.contains("modes"))
        assertTrue(ids.contains("preparation"))
        assertTrue(ids.contains("diagnostics"))
        assertTrue(ids.contains("system"))
        assertTrue(ids.contains("help"))
        assertTrue(ids.contains("calculators"))
    }

    @Test
    fun hasAllEightModes() {
        val modes = byId("modes")?.children?.map { it.id } ?: emptyList()
        assertEquals(
            listOf(
                "mode_distillation", "mode_rectification", "mode_beer", "mode_pvk",
                "mode_nbk", "mode_vacuum", "mode_double", "mode_fermentation",
            ),
            modes,
        )
    }

    @Test
    fun modeItemsResolveToProcessModes() {
        val expected = mapOf(
            "mode_distillation" to ProcessMode.DISTILL,
            "mode_rectification" to ProcessMode.RECTIFY,
            "mode_beer" to ProcessMode.BEER,
            "mode_pvk" to ProcessMode.PVK,
            "mode_nbk" to ProcessMode.NBK,
            "mode_vacuum" to ProcessMode.VACUUM,
            "mode_double" to ProcessMode.DOUBLE,
            "mode_fermentation" to ProcessMode.FERMENT,
        )
        expected.forEach { (id, mode) ->
            val item = byId(id)
            assertNotNull("Missing $id", item)
            val work = byId(id.replace("mode_", "work_"))
            assertNotNull("Missing work screen for $id", work)
            // work screens are HMI submenus; action is mapped and resolvable as start
            val leaf = work!!.copy(type = MenuType.SCREEN, children = emptyList())
            assertEquals(MenuOutcome.Start(mode), MenuActions.resolve(leaf))
        }
    }

    @Test
    fun helpSettingsAndBranchesPresent() {
        // help
        assertNotNull(byId("help"))
        assertTrue((byId("help")?.children?.size ?: 0) >= 1)
        // settings branches exist under modes
        assertNotNull(byId("settings_distillation"))
        assertNotNull(byId("settings_rectification"))
        assertNotNull(byId("settings_beer"))
        assertNotNull(byId("settings_pvk"))
        assertNotNull(byId("settings_nbk"))
        assertNotNull(byId("settings_vacuum"))
        assertNotNull(byId("settings_double"))
        assertNotNull(byId("settings_fermentation"))
        // preparation/diagnostics/system branches
        assertTrue((byId("preparation")?.children?.size ?: 0) >= 1)
        assertTrue((byId("diagnostics")?.children?.size ?: 0) >= 1)
        assertTrue((byId("system")?.children?.size ?: 0) >= 1)
    }

    @Test
    fun allHmiScreensAreReachable() {
        val all = allItems()
        val ids = all.map { it.id }.toSet()
        // every screen from HMI JSON except main should be in the tree
        val hmiIds = HmiMenu.allScreens().map { it.id }.filter { it != "main" }.toSet()
        val missing = hmiIds - ids
        assertTrue("Missing HMI screens in APK tree: $missing", missing.isEmpty())
    }

    @Test
    fun treeHasNoEmptySubmenus() {
        val empties = allItems().filter { it.type == MenuType.SUBMENU && it.children.isEmpty() }
        assertTrue("Empty submenus: ${empties.map { it.id }}", empties.isEmpty())
    }
}
