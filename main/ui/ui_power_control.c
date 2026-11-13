#include "ui_power_control.h"

#include "ui.h"
#include "ui_glass_theme.h"

#include "control/ctrl_heater.h"
#include "control/regulator_driver.h"
#include "core/control_config.h"

#include <math.h>
#include <stdio.h>

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *mode;
    lv_obj_t *hint;
    lv_obj_t *auto_btn;
    lv_timer_t *timer;
} power_ui_t;

static power_ui_t s_ui;

static void update_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_ui.slider) return;

    if (!lv_obj_has_state(s_ui.slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_ui.slider,
            (int16_t)lroundf(ctrl_heater_get_percent()), LV_ANIM_OFF);
    }

    bool manual = ctrl_heater_manual_active();
    lv_label_set_text(s_ui.mode, manual ? "Ручной" : "Авто");
    lv_obj_set_style_text_color(s_ui.mode,
        manual ? UI_GLASS_ORANGE : UI_GLASS_TEXT_2, 0);
    if (manual) lv_obj_clear_state(s_ui.auto_btn, LV_STATE_DISABLED);
    else lv_obj_add_state(s_ui.auto_btn, LV_STATE_DISABLED);

    bool bad = control_config_get_regulator() != REGULATOR_NONE &&
               !regulator_is_ready();
    if (bad) {
        char buf[96];
        int gpio = regulator_get_gpio();
        snprintf(buf, sizeof(buf), "Регулятор не готов (GPIO %d), выход 0", gpio);
        lv_label_set_text(s_ui.hint, buf);
        lv_obj_clear_flag(s_ui.hint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_VALUE_CHANGED) {
        if (code == LV_EVENT_VALUE_CHANGED &&
            !lv_obj_has_state(slider, LV_STATE_PRESSED)) {
            return;
        }
        esp_err_t err = ctrl_heater_manual_set((float)value);
        if (err == ESP_OK) ui_toast("Ручная мощность установлена");
        else ui_toast("Регулятор не готов или защита");
    }
}

static void auto_cb(lv_event_t *e) {
    (void)e;
    ctrl_heater_set_auto();
    ui_toast("Мощность: авто");
}

static void power_delete_cb(lv_event_t *e) {
    (void)e;
    if (s_ui.timer) {
        lv_timer_del(s_ui.timer);
        s_ui.timer = NULL;
    }
    s_ui.slider = s_ui.mode = s_ui.hint = s_ui.auto_btn = NULL;
}

void ui_power_control_attach(lv_obj_t *parent) {
    if (!parent) return;

    if (s_ui.timer) {
        lv_timer_del(s_ui.timer);
        s_ui.timer = NULL;
    }
    s_ui.slider = lv_slider_create(parent);
    lv_obj_set_width(s_ui.slider, lv_pct(100));
    lv_obj_set_height(s_ui.slider, 8);
    lv_slider_set_range(s_ui.slider, 0, 100);
    lv_obj_set_style_bg_color(s_ui.slider, UI_GLASS_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.slider, UI_GLASS_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_ui.slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_add_event_cb(s_ui.slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.slider, slider_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_ui.mode = lv_label_create(row);
    lv_label_set_text(s_ui.mode, "Авто");
    lv_obj_set_style_text_font(s_ui.mode, &lv_font_cyr_16, 0);

    s_ui.auto_btn = lv_btn_create(row);
    lv_obj_set_size(s_ui.auto_btn, 86, 28);
    lv_obj_set_style_radius(s_ui.auto_btn, 8, 0);
    lv_obj_set_style_bg_color(s_ui.auto_btn, UI_GLASS_CARD_2, 0);
    lv_obj_t *al = lv_label_create(s_ui.auto_btn);
    lv_label_set_text(al, "Авто");
    lv_obj_set_style_text_font(al, &lv_font_cyr_16, 0);
    lv_obj_center(al);
    lv_obj_add_event_cb(s_ui.auto_btn, auto_cb, LV_EVENT_CLICKED, NULL);

    s_ui.hint = lv_label_create(parent);
    lv_label_set_long_mode(s_ui.hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_ui.hint, lv_pct(100));
    lv_obj_set_style_text_font(s_ui.hint, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(s_ui.hint, UI_GLASS_RED, 0);
    lv_obj_add_flag(s_ui.hint, LV_OBJ_FLAG_HIDDEN);

    s_ui.timer = lv_timer_create(update_cb, 500, NULL);
    lv_obj_add_event_cb(parent, power_delete_cb, LV_EVENT_DELETE, NULL);
    update_cb(NULL);
}
