#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/recipes_db.h"
#include <string.h>

extern lv_obj_t* ui_make_page(const char *title);

void ui_recipes_list(const char *cat) {
    lv_obj_t *c = ui_make_page("Рецепты");

    static ui_list_item_t items[64];
    int n = 0;
    for (size_t i = 0; i < RECIPES_COUNT && n < 64; i++) {
        if (cat && strstr(RECIPES[i].category, cat) == NULL) continue;
        items[n++] = (ui_list_item_t){
            .icon = "🍺",
            .title = RECIPES[i].name,
            .subtitle = RECIPES[i].description,
            .on_click = [](void *ctx) {
                recipes_show((const recipe_t*)ctx);
            },
            .ctx = (void*)&RECIPES[i]
        };
    }

    if (n == 0) {
        lv_obj_t *l = lv_label_create(c);
        lv_label_set_text(l, "Рецептов не найдено");
        lv_obj_center(l);
        return;
    }
    ui_build_list(c, items, n);
}