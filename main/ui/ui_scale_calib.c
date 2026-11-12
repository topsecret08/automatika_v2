#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "control/ctrl_scale.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static void cb_scale_tare(void *c) {
    (void)c;
    scale_tare_now();
    ui_toast("Тара обнулена");
}

static void cb_scale_cal_100(void *c) {
    (void)c;
    scale_start_calibration(100.0f);
    ui_toast("Положите груз 100 г");
}

void ui_scale_calib(void) {
    lv_obj_t *c = ui_make_page("Калибровка весов");

    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l,
        "1. Уберите всё с весов\n"
        "2. Нажмите «Тара»\n"
        "3. Положите известный груз (100 г)\n"
        "4. Нажмите «Калибровать 100 г»\n"
        "5. Система вычислит коэффициент\n");
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

    static ui_button_t btns[] = {
        {"Тара", "⚖", 0x007AFF, NULL, NULL},
        {"100 г", "🎯", 0x34C759, NULL, NULL},
    };
    btns[0].on_click = cb_scale_tare;
    btns[1].on_click = cb_scale_cal_100;
    ui_build_button_bar(c, btns, 2);
}