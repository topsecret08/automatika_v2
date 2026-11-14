#include "ui_screen_advanced.h"

#include "ui.h"
#include "ui_glass_theme.h"

#include "control/fraction_periodic.h"
#include "control/regulator_driver.h"
#include "control/regulator_settings_logic.h"
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

enum { PWM_FIELD_GPIO = 0, PWM_FIELD_FREQ, PWM_FIELD_RES };

typedef struct {
    int field;
    int dir;
} pwm_step_ctx_t;

static pwm_step_ctx_t s_pwm_steps[] = {
    { PWM_FIELD_GPIO, -1 }, { PWM_FIELD_GPIO, +1 },
    { PWM_FIELD_FREQ, -1 }, { PWM_FIELD_FREQ, +1 },
    { PWM_FIELD_RES,  -1 }, { PWM_FIELD_RES,  +1 },
};

static void pwm_apply_or_toast(regulator_settings_t *rs) {
    esp_err_t err = regulator_apply_settings(rs);
    if (err == ESP_OK) {
        ui_toast("PWM сохранён");
    } else {
        const char *e = regulator_last_error();
        ui_toast(e && e[0] ? e : "PWM не применён; выход 0");
    }
}

static void pwm_step_cb(lv_event_t *event) {
    pwm_step_ctx_t *ctx = (pwm_step_ctx_t *)lv_event_get_user_data(event);
    if (!ctx) return;

    regulator_settings_t rs;
    if (control_config_get_regulator_settings(&rs) != ESP_OK) {
        ui_toast("Настройки недоступны");
        return;
    }

    if (ctx->field == PWM_FIELD_GPIO) {
        int next = regulator_pwm_gpio_next(rs.pwm_gpio, ctx->dir);
        if (next < 0) {
            ui_toast("Нет свободного GPIO");
            return;
        }
        rs.pwm_gpio = (int16_t)next;
    } else if (ctx->field == PWM_FIELD_FREQ) {
        int32_t step = (rs.pwm_freq_hz < 100) ? 10 : 100;
        int32_t f = (int32_t)rs.pwm_freq_hz + ctx->dir * step;
        if (f < 1) f = 1;
        if (f > (int32_t)REG_PWM_MAX_FREQ_HZ) f = REG_PWM_MAX_FREQ_HZ;
        rs.pwm_freq_hz = (uint32_t)f;
    } else {
        int r = (int)rs.pwm_resolution + ctx->dir;
        if (r < (int)REG_PWM_MIN_RES) r = REG_PWM_MIN_RES;
        if (r > (int)REG_PWM_MAX_RES) r = REG_PWM_MAX_RES;
        rs.pwm_resolution = (uint8_t)r;
    }

    pwm_apply_or_toast(&rs);
    ui_screen_advanced_create();
}

static void pwm_polarity_cb(lv_event_t *event) {
    (void)event;
    regulator_settings_t rs;
    if (control_config_get_regulator_settings(&rs) != ESP_OK) {
        ui_toast("Настройки недоступны");
        return;
    }
    rs.pwm_inverted = !rs.pwm_inverted;
    pwm_apply_or_toast(&rs);
    ui_screen_advanced_create();
}

static lv_obj_t *make_pwm_row(lv_obj_t *parent, const char *title,
                              pwm_step_ctx_t *minus, pwm_step_ctx_t *plus) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 36);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_font(l, &lv_font_cyr_16, 0);

    lv_obj_t *box = lv_obj_create(row);
    lv_obj_set_size(box, 120, 32);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *m = lv_btn_create(box);
    lv_obj_set_size(m, 34, 28);
    lv_obj_t *ml = lv_label_create(m);
    lv_label_set_text(ml, "−");
    lv_obj_center(ml);
    lv_obj_add_event_cb(m, pwm_step_cb, LV_EVENT_CLICKED, minus);

    lv_obj_t *p = lv_btn_create(box);
    lv_obj_set_size(p, 34, 28);
    lv_obj_t *pl = lv_label_create(p);
    lv_label_set_text(pl, "+");
    lv_obj_center(pl);
    lv_obj_add_event_cb(p, pwm_step_cb, LV_EVENT_CLICKED, plus);

    return row;
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
    make_choice(card, "SSR / MOSFET (ШИМ)", reg == REGULATOR_SSR_PWM,
                regulator_cb, REGULATOR_SSR_PWM);

    regulator_settings_t rs;
    control_config_get_regulator_settings(&rs);

    lv_obj_t *status = lv_label_create(card);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status, lv_pct(100));
    char sbuf[160];
    if (reg == REGULATOR_NONE) {
        snprintf(sbuf, sizeof(sbuf),
                 "Внутренний DAC · внешний регулятор не выбран");
    } else {
        snprintf(sbuf, sizeof(sbuf), "%s · %s\nGPIO %d · %lu Гц · %u бит · %s",
                 control_config_regulator_name(reg),
                 regulator_is_ready() ? "готов" : "НЕ ГОТОВ",
                 regulator_get_gpio(), (unsigned long)rs.pwm_freq_hz,
                 (unsigned)rs.pwm_resolution,
                 rs.pwm_inverted ? "активный низкий" : "активный высокий");
    }
    lv_label_set_text(status, sbuf);
    lv_obj_set_style_text_font(status, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(status,
        regulator_is_ready() || reg == REGULATOR_NONE
            ? UI_GLASS_TEXT_2 : UI_GLASS_RED, 0);

    if (reg == REGULATOR_SSR_PWM) {
        char buf[48];
        snprintf(buf, sizeof(buf), "PWM GPIO: %d", (int)rs.pwm_gpio);
        make_pwm_row(card, buf, &s_pwm_steps[0], &s_pwm_steps[1]);

        snprintf(buf, sizeof(buf), "Частота: %lu Гц",
                 (unsigned long)rs.pwm_freq_hz);
        make_pwm_row(card, buf, &s_pwm_steps[2], &s_pwm_steps[3]);

        snprintf(buf, sizeof(buf), "Разрядность: %u бит",
                 (unsigned)rs.pwm_resolution);
        make_pwm_row(card, buf, &s_pwm_steps[4], &s_pwm_steps[5]);

        lv_obj_t *pol = lv_btn_create(card);
        lv_obj_set_width(pol, lv_pct(100));
        lv_obj_set_height(pol, 34);
        lv_obj_t *pl = lv_label_create(pol);
        lv_label_set_text(pl, rs.pwm_inverted
                                  ? "Полярность: активный низкий (нажмите)"
                                  : "Полярность: активный высокий (нажмите)");
        lv_obj_set_style_text_font(pl, &lv_font_cyr_16, 0);
        lv_obj_center(pl);
        lv_obj_add_event_cb(pol, pwm_polarity_cb, LV_EVENT_CLICKED, NULL);
    }

    if (regulator_last_error()[0]) {
        lv_obj_t *err = lv_label_create(card);
        lv_label_set_long_mode(err, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(err, lv_pct(100));
        lv_label_set_text(err, regulator_last_error());
        lv_obj_set_style_text_font(err, &lv_font_cyr_16, 0);
        lv_obj_set_style_text_color(err, UI_GLASS_RED, 0);
    }

    lv_obj_t *hint = lv_label_create(card);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_text(hint,
        "UART и силовой SSR подключайте только через оптронную развязку. "
        "GPIO по умолчанию -1: задайте реально свободный вывод платы. "
        "5 кГц подходит не всем входам; для AC SSR нужен zero-cross/иной режим.");
    lv_obj_set_style_text_font(hint, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(hint, UI_GLASS_ORANGE, 0);
}
