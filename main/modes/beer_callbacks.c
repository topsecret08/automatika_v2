#include "beer_recipe.h"
#include "ui/ui.h"
#include "esp_log.h"

static int s_edit_idx = -1;
static int s_edit_step = -1;

void cb_beer_edit_recipe(void *ctx) {
    int idx = (int)(intptr_t)ctx;
    s_edit_idx = idx;
    extern void ui_beer_editor_steps(int recipe_idx);
    ui_beer_editor_steps(idx);
}

void cb_beer_add_step(void *ctx) {
    (void)ctx;
    beer_recipe_t *r = beer_recipe_get(s_edit_idx);
    if (!r || r->step_count >= BEER_MAX_STEPS) return;
    beer_step_t *s = &r->steps[r->step_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "Этап %d", r->step_count);
    s->temp = 65.0f;
    s->duration_sec = 1800;
    extern void ui_beer_editor_steps(int);
    ui_beer_editor_steps(s_edit_idx);
}

void cb_beer_save(void *ctx) {
    (void)ctx;
    beer_recipe_t *r = beer_recipe_get(s_edit_idx);
    if (r) {
        beer_recipe_save(r);
        ui_toast("Рецепт сохранён");
    }
}

void cb_beer_start_recipe(void *ctx) {
    (void)ctx;
    beer_recipe_start(s_edit_idx);
    ui_toast("Пивоварение запущено");
    ui_show_process();
}

void cb_beer_save_step(void *ctx) {
    (void)ctx;
    beer_recipe_t *r = beer_recipe_get(s_edit_idx);
    if (r) beer_recipe_save(r);
    ui_toast("Сохранено");
}

void cb_beer_del_step(void *ctx) {
    (void)ctx;
    beer_recipe_t *r = beer_recipe_get(s_edit_idx);
    if (!r || s_edit_step < 0) return;
    for (int i = s_edit_step; i < r->step_count - 1; i++)
        r->steps[i] = r->steps[i + 1];
    r->step_count--;
    extern void ui_beer_editor_steps(int);
    ui_beer_editor_steps(s_edit_idx);
}