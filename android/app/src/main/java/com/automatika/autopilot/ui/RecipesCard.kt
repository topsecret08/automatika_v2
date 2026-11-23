package com.automatika.autopilot.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.automatika.autopilot.data.ControllerParser
import com.automatika.autopilot.data.RecipeDetail
import com.automatika.autopilot.data.RecipeSummary
import java.net.URLEncoder

/** Рецепты одной категории: список, затем рецепт целиком. Данные из /api/recipes и /api/recipe. */
@Composable
fun RecipesCard(category: String, title: String, vm: AutopilotViewModel) {
    var list by remember { mutableStateOf<List<RecipeSummary>>(emptyList()) }
    var detail by remember { mutableStateOf<RecipeDetail?>(null) }
    var status by remember { mutableStateOf("Нажмите «Обновить».") }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            val current = detail
            if (current == null) {
                OutlinedButton(onClick = {
                    vm.execute { api ->
                        list = ControllerParser.recipes(api.getArray("/api/recipes")).filter { it.category == category }
                        status = if (list.isEmpty()) "Рецептов нет." else ""
                    }
                }) { Text("Обновить") }
                if (status.isNotBlank()) Text(status, style = MaterialTheme.typography.bodySmall)
                list.forEach { recipe ->
                    Button(onClick = {
                        vm.execute { api ->
                            val encoded = URLEncoder.encode(recipe.id, "UTF-8")
                            detail = ControllerParser.recipe(api.getJson("/api/recipe?id=$encoded"))
                        }
                    }) { Text(recipe.name) }
                    Text(recipe.description, style = MaterialTheme.typography.bodySmall)
                }
            } else {
                Text(current.name, style = MaterialTheme.typography.titleSmall)
                Text(current.description, style = MaterialTheme.typography.bodySmall)
                Text("Ингредиенты", style = MaterialTheme.typography.labelLarge)
                Text(current.ingredients, style = MaterialTheme.typography.bodyMedium)
                Text("Шаги", style = MaterialTheme.typography.labelLarge)
                Text(current.stepsMd, style = MaterialTheme.typography.bodyMedium)
                OutlinedButton(onClick = { detail = null }) { Text("К списку") }
            }
        }
    }
}
