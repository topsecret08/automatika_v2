#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "control/ctrl_scale.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);

void ui_scale_main(void) {
    lv_obj_t *c = ui_make_page("Весы HX711");
    scale_t *s = scale_get();

    static char w[16], fl[24], tt[16], st[16];
    snprintf(w,  sizeof(w),  "%.1f г", s->grams_filtered);
    snprintf(fl, sizeof(fl), "%.1f мл/мин", s->flow_ml_per_min);
    snprintf(tt, sizeof(tt), "%.1f мл", s->total_ml);
    snprintf(st, sizeof(st), "%s", s->stable ? "СТАБ" : "…");

    static ui_card_t cards[] = {
        {"⚖","Вес",      w,  NULL, NULL, 0x007AFF, NULL, NULL},
        {"💧","Поток",   fl, NULL, NULL, 0x34C759, NULL, NULL},
        {"📊","Итого",   tt, NULL, NULL, 0xFF9500, NULL, NULL},
        {"🎯","Статус",  st, NULL, NULL, 0xAF52DE, NULL, NULL},
    };
    ui_build_card_grid(c, cards, 4);

    static ui_button_t btns[] = {
        {"Тара", "⚖", 0x007AFF, NULL, NULL},
        {"Сброс", "🔄", 0xFF9500, NULL, NULL},
        {"Плотность", "💧", 0x34C759, NULL, NULL},
    };
    extern void cb_scale_tare(void*);
    extern void cb_scale_density(void*);
    btns[0].on_click = cb_scale_tare;
    btns[1].on_click = [](void*){ scale_reset_session(); };
    btns[2].on_click = cb_scale_density;
    ui_build_button_bar(c, btns, 3);
}