package com.automatika.autopilot.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/**
 * Статические данные, вшитые в APK (assets/static, см. tools/gen_android_assets.py):
 * справка и каталог рецептов. Они не меняются от устройства к устройству,
 * поэтому не запрашиваются у контроллера. Динамика (состояние, параметры,
 * журналы, шаги рецепта, если их нет в каталоге) по-прежнему идёт по API.
 */
object StaticContent {
    @Volatile
    var appContext: Context? = null

    /** Категория приложения → файл каталога. Категории совпадают с пунктами меню. */
    private val recipeFiles = linkedMapOf(
        "beer" to "beer.json",
        "spirit" to "spirits.json",
        "mash" to "mash.json",
    )

    private fun read(path: String): String? {
        val ctx = appContext ?: return null
        return runCatching {
            ctx.assets.open("static/$path").bufferedReader(Charsets.UTF_8).use { it.readText() }
        }.getOrNull()
    }

    fun help(id: String): HelpArticle? {
        if (id.isBlank() || id.contains('/') || id.contains("..")) return null
        val text = read("help/$id.md") ?: return null
        return HelpArticle(id = id, title = headingOf(text, id), contentMd = text)
    }

    fun recipes(category: String): List<RecipeSummary> {
        val file = recipeFiles[category] ?: return emptyList()
        val json = read("recipes/$file") ?: return emptyList()
        return parseRecipes(json, category)
    }

    /** Рецепт целиком из каталога; шаги приготовления в каталоге не хранятся (пустая строка). */
    fun recipe(id: String): RecipeDetail? {
        for ((category, file) in recipeFiles) {
            val json = read("recipes/$file") ?: continue
            val array = JSONArray(json)
            for (index in 0 until array.length()) {
                val item = array.optJSONObject(index) ?: continue
                if (item.optString("id") == id) return parseRecipe(item, category)
            }
        }
        return null
    }

    internal fun parseRecipes(json: String, category: String): List<RecipeSummary> {
        val array = JSONArray(json)
        return (0 until array.length()).mapNotNull { index ->
            val item = array.optJSONObject(index) ?: return@mapNotNull null
            RecipeSummary(
                id = item.optString("id", ""),
                name = item.optString("name", ""),
                category = category,
                description = item.optString("description", ""),
            ).takeIf { it.id.isNotBlank() }
        }
    }

    internal fun parseRecipe(item: JSONObject, category: String): RecipeDetail = RecipeDetail(
        id = item.optString("id", ""),
        name = item.optString("name", ""),
        category = category,
        description = item.optString("description", ""),
        ingredients = item.optString("ingredients", item.optString("ing", "")),
        stepsMd = "",
    )

    /** Заголовок статьи: первая строка «# …» или идентификатор, если заголовка нет. */
    internal fun headingOf(markdown: String, fallback: String): String {
        val first = markdown.lineSequence().map { it.trim() }.firstOrNull { it.isNotEmpty() }
        return first?.takeIf { it.startsWith("#") }?.trimStart('#', ' ')?.ifBlank { fallback } ?: fallback
    }
}
