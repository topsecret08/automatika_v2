#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/learning.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);

void ui_learning_show_lessons(const void *module) {
    const module_t *m = (const module_t*)module;
    if (!m) return;

    lv_obj_t *c = ui_make_page(m->name);

    static ui_list_item_t items[16];
    for (size_t i = 0; i < m->count && i < 16; i++) {
        items[i] = (ui_list_item_t){
            .icon = "📖",
            .title = m->lessons[i].title,
            .subtitle = m->lessons[i].module,
            .on_click = [](void *ctx) {
                ui_learning_quiz((const lesson_t*)ctx);
            },
            .ctx = (void*)&m->lessons[i]
        };
    }
    ui_build_list(c, items, m->count);
}

void ui_learning_quiz(const void *lesson) {
    const lesson_t *l = (const lesson_t*)lesson;
    if (!l) return;

    lv_obj_t *c = ui_make_page(l->title);

    lv_obj_t *theory = lv_label_create(c);
    lv_label_set_text(theory, l->theory_md);
    lv_obj_set_width(theory, LV_PCT(100));
    lv_label_set_long_mode(theory, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(theory, &lv_font_montserrat_12, 0);
    lv_obj_align(theory, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *q = lv_label_create(c);
    lv_label_set_text(q, l->quiz_q);
    lv_obj_set_style_text_font(q, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(q, IOS_BLUE, 0);
    lv_obj_set_width(q, LV_PCT(100));
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_align(q, LV_ALIGN_BOTTOM_LEFT, 0, -160);

    static ui_list_item_t items[4];
    for (int i = 0; i < 4; i++) {
        items[i] = (ui_list_item_t){
            .icon = "▶", .title = l->quiz_a[i],
            .on_click = [](void *ctx) {
                int idx = (int)(intptr_t)ctx;
                extern void ui_learning_answer(int);
                ui_learning_answer(idx);
            },
            .ctx = (void*)(intptr_t)i
        };
    }

    lv_obj_t *list = lv_obj_create(c);
    lv_obj_set_size(list, LV_PCT(100), 140);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    ui_build_list(list, items, 4);
}

void ui_learning_answer(int idx) {
    (void)idx;
    extern void ui_toast(const char*);
    ui_toast("Ответ принят");
}