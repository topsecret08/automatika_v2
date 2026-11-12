#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "modes/beer_recipe.h"
#include <stdio.h>
#include <string.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void cb_beer_edit_recipe(void*);
extern void cb_beer_add_step(void*);
extern void cb_beer_save(void*);
extern void cb_beer_start_recipe(void*);

static void cb_new_beer_recipe(lv_event_t *e) {
    (void)e;
    beer_recipe_new();
    ui_beer_editor_list();
}

static void cb_edit_beer_step(lv_event_t *e) {
    (void)e;
    ui_beer_editor_step_edit();
}

void ui_beer_editor_list(void) {
    lv_obj_t *c = ui_make_page("Мои рецепты пива");
    beer_recipe_t *recipes;
    int n = beer_recipe_list(&recipes);

    static ui_list_item_t items[20];
    static char subs[20][64];

    for (int i = 0; i < n; i++) {
        snprintf(subs[i], sizeof(subs[i]), "%.0f л · %d этапов",
                 recipes[i].batch_size_l, recipes[i].step_count);
        items[i] = (ui_list_item_t){
            .icon = "🍺",
            .title = recipes[i].name,
            .subtitle = subs[i],
            .on_click = cb_beer_edit_recipe,
            .ctx = (void*)(intptr_t)i
        };
    }
    ui_build_list(c, items, n);

    // Кнопка "Создать"
    lv_obj_t *btn = lv_btn_create(c);
    lv_obj_set_size(btn, 160, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    ui_style_apply_btn_primary(btn);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, "＋ Создать");
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, cb_new_beer_recipe, LV_EVENT_CLICKED, NULL);
}

void ui_beer_editor_steps(int recipe_idx) {
    beer_recipe_t *r = beer_recipe_get(recipe_idx);
    if (!r) return;

    lv_obj_t *c = ui_make_page(r->name);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 4, 0);

    for (int i = 0; i < r->step_count; i++) {
        beer_step_t *st = &r->steps[i];
        lv_obj_t *row = lv_btn_create(c);
        lv_obj_set_size(row, LV_PCT(100), 56);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        char buf[96];
        snprintf(buf, sizeof(buf), "%d. %s\n%.0f°C · %u мин",
                 i + 1, st->name, st->temp, (unsigned)(st->duration_sec / 60));
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_add_event_cb(row, cb_edit_beer_step, LV_EVENT_CLICKED, NULL);
    }

    static ui_button_t btns[] = {
        {"＋ Этап", "➕", 0x34C759, NULL, NULL},
        {"Сохранить", "💾", 0x007AFF, NULL, NULL},
        {"Запустить", "▶", 0xFF9500, NULL, NULL},
    };
    btns[0].on_click = cb_beer_add_step;
    btns[1].on_click = cb_beer_save;
    btns[2].on_click = cb_beer_start_recipe;
    ui_build_button_bar(c, btns, 3);
}

void ui_beer_editor_step_edit(void) {
    // Заглушка — можно расширить
    extern void ui_toast(const char*);
    ui_toast("Редактор этапа");
}