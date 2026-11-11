#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "control/valve_calibration.h"
#include "core/valve_config.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

void ui_valve_calib_menu(void) {
    lv_obj_t *c = ui_make_page("Калибровка клапанов");

    static ui_list_item_t items[6];
    static char subs[6][96];

    for (int i = 0; i < 3; i++) {
        valve_config_t *v = valve_config_get(i);
        snprintf(subs[i], sizeof(subs[i]), "%s — %.1f мл/ч",
                 v->norm == VALVE_NORMALLY_CLOSED ? "НЗ" : "НО",
                 v->ml_per_hour);
        items[i] = (ui_list_item_t){
            .icon = "🎚", .title = v->name,
            .subtitle = subs[i],
            .on_click = [](void *ctx) { ui_valve_calib_run((int)(intptr_t)ctx); },
            .ctx = (void*)(intptr_t)i
        };
    }

    items[3] = (ui_list_item_t){
        .icon = "🗑", .title = "Сбросить калибровку",
        .is_destructive = 1,
        .on_click = [](void*) {
            for (int i = 0; i < 3; i++) {
                valve_config_t *v = valve_config_get(i);
                v->calibrated = false;
                v->ml_per_hour = 0;
            }
            valve_config_save();
            ui_toast("Калибровка сброшена");
        }
    };
    ui_build_list(c, items, 4);
}

void ui_valve_calib_run(int idx) {
    valve_config_t *v = valve_config_get(idx);
    if (!v) return;

    lv_obj_t *c = ui_make_page("Калибровка клапана");
    lv_obj_t *l = lv_label_create(c);
    char msg[512];
    snprintf(msg, sizeof(msg),
        "Клапан: %s\nТип: %s\n\n"
        "1. Нажмите СТАРТ\n"
        "2. Клапан будет открываться по программе\n"
        "3. Соберите продукт в мерный цилиндр\n"
        "4. Введите фактический объём\n\n"
        "Калибровка идёт в фоне — процесс не прерывается!",
        v->name, v->norm == VALVE_NORMALLY_CLOSED ? "НЗ" : "НО");
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, msg);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

    static ui_button_t btns[] = {
        {"СТАРТ", "▶", 0x34C759, NULL, NULL},
        {"СТОП", "⏹", 0xFF3B30, NULL, NULL},
    };
    btns[0].on_click = [](void *ctx) {
        int idx = (int)(intptr_t)ctx;
        valve_calib_start(idx, 100.0f);
        ui_toast("Калибровка запущена");
    };
    btns[0].ctx = (void*)(intptr_t)idx;
    btns[1].on_click = [](void *ctx) {
        int idx = (int)(intptr_t)ctx;
        valve_calib_finish(idx, 100.0f);
        ui_toast("Калибровка завершена");
    };
    btns[1].ctx = (void*)(intptr_t)idx;
    ui_build_button_bar(c, btns, 2);
}

void ui_valve_calib_progress(int idx) {
    (void)idx;
    // Заглушка
}