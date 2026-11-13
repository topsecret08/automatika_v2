#include "ui_screen_advanced.h"

#include "ui.h"
#include "ui_glass_theme.h"

#include "control/fraction_periodic.h"
#include "control/regulator_driver.h"
#include "core/control_config.h"

#include <stdio.h>

extern lv_obj_t *ui_make_page(const char *title);

static lv_obj_t *s_root;

static lv_obj_t *make_choice(lv_obj_t *parent, const char *text,
                             bool active, lv_event_cb_t cb, intptr_t value) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, 38);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, active ? UI_GLASS_ACCENT : UI_GLASS_CARD_2, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_left(btn, 12, 0);
    lv_obj_set_style_pad_right(btn, 12, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);

    lv_obj_t *mark = lv_label_create(btn);
    lv_label_set_text(mark, active ? LV_SYMBOL_OK : "");
    lv_obj_set_style_text_font(mark, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(mark, lv_color_white(), 0);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void *)value);
    return btn;
}

static void algo_cb(lv_event_t *event) {
    select_algo_t algo = (select_algo_t)(intptr_t)lv_event_get_user_data(event);
    if (fsel_set_algo(algo) == ESP_OK) ui_toast("Алгоритм сохранён");
    else ui_toast("Не удалось сохранить алгоритм");
    ui_screen_advanced_create();
}

static void regulator_cb(lv_event_t *event) {
    regulator_type_t type =
        (regulator_type_t)(intptr_t)lv_event_get_user_data(event);
    if (regulator_select(type) == ESP_OK) {
        ui_toast("Регулятор сохранён");
    } else {
        ui_toast("Регулятор не готов; выход 0");
    }
    ui_screen_advanced_create();
}

static void adaptive_cb(lv_event_t *event) {
    bool on = (bool)(intptr_t)lv_event_get_user_data(event);
    if (control_config_set_adaptive_pid(on) == ESP_OK)
        ui_toast(on ? "Адаптивный PID включён" : "Адаптивный PID выключен");
    else
        ui_toast("Не удалось сохранить");
    ui_screen_advanced_create();
}

static void section(lv_obj_t *parent, const char *title) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_font(l, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(l, UI_GLASS_TEXT_2, 0);
}

void ui_screen_advanced_create(void) {
    s_root = ui_make_page("Отбор и регулятор");
    if (!s_root) return;

    select_algo_t algo = fsel_get_algo();
    regulator_type_t reg = control_config_get_regulator();

    lv_obj_t *card = s_root;
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    section(card, "АЛГОРИТМ ОТБОРА");    make_choice(card, "Классический", algo == ALGO_CLASSIC,
                algo_cb, ALGO_CLASSIC);
    make_choice(card, "Тренд", algo == ALGO_TREND,
                algo_cb, ALGO_TREND);
    make_choice(card, "Адаптивный", algo == ALGO_ADAPTIVE,
                algo_cb, ALGO_ADAPTIVE);

    section(card, "МНОГОУРОВНЕВЫЙ PID");
    bool ad = control_config_get_adaptive_pid();
    make_choice(card, "Выключен", !ad, adaptive_cb, 0);
    make_choice(card, "Включён (после verified)", ad, adaptive_cb, 1);

    section(card, "ВНЕШНИЙ РЕГУЛЯТОР");
    make_choice(card, "Нет (внутренний DAC)", reg == REGULATOR_NONE,
                regulator_cb, REGULATOR_NONE);
    make_choice(card, "KVIC (UART)", reg == REGULATOR_KVIC,
                regulator_cb, REGULATOR_KVIC);
    make_choice(card, "РМВ-К (UART)", reg == REGULATOR_RMVK,
                regulator_cb, REGULATOR_RMVK);
    make_choice(card, "SEM (UART)", reg == REGULATOR_SEM_AVR,
                regulator_cb, REGULATOR_SEM_AVR);
    make_choice(card, "SSR (ШИМ 5 кГц)", reg == REGULATOR_SSR_PWM,
                regulator_cb, REGULATOR_SSR_PWM);

    lv_obj_t *status = lv_label_create(card);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status, lv_pct(100));
    char sbuf[128];
    snprintf(sbuf, sizeof(sbuf), "%s · %s · GPIO %d · %d Гц",
             control_config_regulator_name(reg),
             regulator_is_ready() ? "готов" : "НЕ ГОТОВ",
             regulator_get_gpio(), regulator_get_pwm_freq());
    lv_label_set_text(status, sbuf);
    lv_obj_set_style_text_font(status, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(status,
        regulator_is_ready() || reg == REGULATOR_NONE
            ? UI_GLASS_TEXT_2 : UI_GLASS_RED, 0);

    lv_obj_t *hint = lv_label_create(card);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_text(hint,
        "UART подключайте только через оптронную развязку. "
        "Пины задаются в config.h; при -1 драйвер не активен.");
    lv_obj_set_style_text_font(hint, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(hint, UI_GLASS_ORANGE, 0);
}
