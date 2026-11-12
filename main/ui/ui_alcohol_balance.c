#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "control/ctrl_alcohol_balance.h"
#include <stdio.h>
#include <stdlib.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static lv_obj_t *s_vol_ta = NULL;
static lv_obj_t *s_abv_ta = NULL;

static void cb_ab_input(void *c) {
    (void)c;
    ui_alcohol_balance_input();
}

static void cb_ab_accept(void *c) {
    (void)c;
    ui_toast("Отбор учтён");
}

static void cb_ab_save(void *c) {
    (void)c;
    float v = atof(lv_textarea_get_text(s_vol_ta));
    float a = atof(lv_textarea_get_text(s_abv_ta));
    alcohol_balance_set_initial(v, a);
    ui_toast("Сохранено");
    ui_alcohol_balance_show();
}

void ui_alcohol_balance_show(void) {
    lv_obj_t *c = ui_make_page("Баланс спирта");
    alcohol_balance_t *a = alcohol_balance_get();

    static char vol[16], abv[16], rem[16], pct[16];
    snprintf(vol, sizeof(vol), "%.1f л", a->initial_volume_l);
    snprintf(abv, sizeof(abv), "%.1f%%", a->initial_abv);
    snprintf(rem, sizeof(rem), "%.2f л", a->remaining_alcohol_l);
    snprintf(pct, sizeof(pct), "%.0f%%", a->remaining_pct);

    static ui_card_t cards[] = {
        {"🛢","Объём",   vol, NULL, NULL, 0x007AFF, NULL, NULL},
        {"💧","Крепость",abv, NULL, NULL, 0x34C759, NULL, NULL},
        {"🍶","Остаток", rem, NULL, NULL, 0xFF9500, NULL, NULL},
        {"📊","Доля",    pct, NULL, NULL, 0xAF52DE, NULL, NULL},
    };
    ui_build_card_grid(c, cards, 4);

    static ui_button_t btns[] = {
        {"Задать", "⚙", 0x007AFF, NULL, NULL},
        {"Отбор",  "📤", 0x34C759, NULL, NULL},
    };
    btns[0].on_click = cb_ab_input;
    btns[1].on_click = cb_ab_accept;
    ui_build_button_bar(c, btns, 2);
}

void ui_alcohol_balance_input(void) {
    lv_obj_t *c = ui_make_page("Ввод данных");

    lv_obj_t *l1 = lv_label_create(c);
    lv_label_set_text(l1, "Объём браги (л):");
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *ta1 = lv_textarea_create(c);
    lv_textarea_set_one_line(ta1, true);
    lv_obj_set_size(ta1, 200, 40);
    lv_obj_align(ta1, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_textarea_set_text(ta1, "20");
    s_vol_ta = ta1;

    lv_obj_t *l2 = lv_label_create(c);
    lv_label_set_text(l2, "Крепость браги (%):");
    lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 0, 90);

    lv_obj_t *ta2 = lv_textarea_create(c);
    lv_textarea_set_one_line(ta2, true);
    lv_obj_set_size(ta2, 200, 40);
    lv_obj_align(ta2, LV_ALIGN_TOP_LEFT, 0, 120);
    lv_textarea_set_text(ta2, "12");
    s_abv_ta = ta2;

    static ui_button_t btns[] = {
        {"✓ Сохранить", NULL, 0x34C759, NULL, NULL},
    };
    btns[0].on_click = cb_ab_save;
    ui_build_button_bar(c, btns, 1);
}