#include "ui_session_indicator.h"

#include "ui.h"
#include "ui_glass_theme.h"
#include "ui_screen_home.h"

#include "core/session_manager.h"
#include "core/state.h"
#include "control/fraction_periodic.h"
#include "hal/baro.h"
#include "hal/hal_1wire.h"

#include <math.h>
#include <stdio.h>

static lv_obj_t *s_btn = NULL;
static lv_obj_t *s_label = NULL;

static void tap_cb(lv_event_t *event) {
    (void)event;
    session_restore();
    ui_screen_home_create();
}

void ui_session_indicator_init(void) {
    if (s_btn) return;

    lv_obj_t *top = lv_layer_top();
    s_btn = lv_btn_create(top);
    lv_obj_set_size(s_btn, 300, 34);
    lv_obj_align(s_btn, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_bg_color(s_btn, UI_GLASS_ACCENT, 0);
    lv_obj_set_style_bg_opa(s_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_btn, 12, 0);
    lv_obj_set_style_shadow_width(s_btn, 10, 0);
    lv_obj_set_style_shadow_opa(s_btn, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(s_btn, lv_color_black(), 0);
    lv_obj_add_flag(s_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_btn, tap_cb, LV_EVENT_CLICKED, NULL);

    s_label = lv_label_create(s_btn);
    lv_label_set_text(s_label, "РАБОТА →");
    lv_obj_set_style_text_font(s_label, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_obj_center(s_label);
}

void ui_session_indicator_update(void) {
    if (!s_btn) return;

    if (!session_is_running() || ui_is_dashboard_visible()) {
        lv_obj_add_flag(s_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    process_state_t *st = state_get();
    float steam = hal_1wire_read_fast(1);
    if (!isfinite(steam) || steam < -50.0f) steam = st->column_temp;
    steam = baro_correct_boiling_point(steam, baro_get_pressure_hpa());

    char buf[96];
    snprintf(buf, sizeof(buf), "● %s · %.1f° →",
             state_stage_name(st->stage), (double)steam);
    lv_label_set_text(s_label, buf);
    lv_obj_clear_flag(s_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_btn);
}
