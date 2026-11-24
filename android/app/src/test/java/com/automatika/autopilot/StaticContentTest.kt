package com.automatika.autopilot

import com.automatika.autopilot.data.StaticContent
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class StaticContentTest {
    private val beerJson = """
        [{"id":"beer.helles","name":"Хеллес","category":"beer","description":"Светлое"},
         {"id":"","name":"без id","category":"beer","description":""}]
    """.trimIndent()

    private val spiritsJson = """
        [{"id":"sp.whisky","name":"Виски","ing":"Ячмень 5 кг"}]
    """.trimIndent()

    @Test
    fun parsesBeerWithDescriptionAndSkipsEntriesWithoutId() {
        val list = StaticContent.parseRecipes(beerJson, "beer")
        assertEquals(1, list.size)
        assertEquals("beer.helles", list[0].id)
        assertEquals("Светлое", list[0].description)
        assertEquals("beer", list[0].category)
    }

    @Test
    fun categoryComesFromFileNotFromEntry() {
        val list = StaticContent.parseRecipes(spiritsJson, "spirit")
        assertEquals("spirit", list[0].category)
        assertEquals("", list[0].description)
    }

    @Test
    fun ingredientsFallBackToIngField() {
        val recipe = StaticContent.parseRecipe(JSONObject(spiritsJson.removePrefix("[").removeSuffix("]")), "spirit")
        assertEquals("Ячмень 5 кг", recipe.ingredients)
        assertEquals("", recipe.stepsMd)
    }

    @Test
    fun headingIsFirstMarkdownHeadingOrFallback() {
        assertEquals("Основы", StaticContent.headingOf("# Основы\n\nТекст", "01_intro"))
        assertEquals("01_intro", StaticContent.headingOf("Текст без заголовка", "01_intro"))
        assertTrue(StaticContent.headingOf("", "x") == "x")
    }
}
