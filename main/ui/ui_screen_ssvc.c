#include "ui_screen_ssvc.h"
#include "ui.h"
#include "ui_menu.h"
#include "ui_ios_theme.h"

#include "control/fraction_ssvc.h"

#include <stdio.h>

static lv_obj_t *s_stage;
static lv_obj_t *s_tfix;
static lv_obj_t *s_open;
static lv_obj_t *s_close;
static lv_obj_t *s_countdown;
static lv_obj_t *s_valve_dot;
static lv_obj_t *s_valve_text;
static lv_obj_t *s_ethanol;
static lv_obj_t *s_state;
static lv_timer_t *s_timer;
static lv_timer_t *s_pulse_timer;

// ------------------------------------------------------------------
// Модальный редактор параметра
// ------------------------------------------------------------------
typedef enum {
    P_TFIX = 0,
    P_OPEN,
    P_CLOSE,
} ssvc_param_id_t;

static lv_obj_t *s_modal;
static lv_obj_t *s_modal_value;
static ssvc_param_id_t s_modal_param;
static float s_modal_edit;

static const char *param_label(ssvc_param_id_t id) {
    switch (id) {
    case P_TFIX:  return "T фиксации, °C";
    case P_OPEN:  return "Время открыт, с";
    case P_CLOSE: return "Время закрыт, с";
    default:      return "Параметр";
    }
}

static const char *param_name(ssvc_param_id_t id) {
    switch (id) {
    case P_TFIX:  return "t_fix";
    case P_OPEN:  return "open_time_sec";
    case P_CLOSE: return "close_time_sec";
    default:      return "";
    }
}

static float param_step(ssvc_param_id_t id) {
    return (id == P_TFIX) ? 0.1f : 0.1f;
}

static float param_min(ssvc_param_id_t id) {
    return (id == P_TFIX) ? 0.0f : 0.0f;
}

static float param_max(ssvc_param_id_t id) {
    return (id == P_TFIX) ? 150.0f : 3600.0f;
}

static float param_current(ssvc_param_id_t id) {
    ssvc_state_t st;
    ssvc_get_state(&st);
    switch (id) {
    case P_TFIX:  return st.t_fix;
    case P_OPEN:  return st.open_time_sec;
    case P_CLOSE: return st.close_time_sec;
    default:      return 0.0f;
    }
}

static void modal_update_value(void) {
    if (!s_modal_value) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", (double)s_modal_edit);
    lv_label_set_text(s_modal_value, buf);
}

static void modal_close(void) {
    if (s_modal) {
        lv_obj_del(s_modal);
        s_modal = NULL;
        s_modal_value = NULL;
    }
}

static void modal_cancel_cb(lv_event_t *event) {
    (void)event;
    modal_close();
}

static void modal_step_cb(lv_event_t *event) {
    float dir = (float)(intptr_t)lv_event_get_user_data(event);
    s_modal_edit += dir * param_step(s_modal_param);
    if (s_modal_edit < param_min(s_modal_param)) s_modal_edit = param_min(s_modal_param);
    if (s_modal_edit > param_max(s_modal_param)) s_modal_edit = param_max(s_modal_param);
    modal_update_value();
}

static void modal_ok_cb(lv_event_t *event) {
    (void)event;
    ssvc_set_param(param_name(s_modal_param), s_modal_edit);
    modal_close();
    ui_toast("Параметр сохранён");
}

static void open_edit_modal(ssvc_param_id_t id) {
    modal_close();

    s_modal_param = id;
    s_modal_edit = param_current(id);

    s_modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_modal, 340, 200);
    lv_obj_center(s_modal);
    lv_obj_set_style_bg_color(s_modal, UI_IOS_CARD, 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_modal, 20, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_set_style_pad_all(s_modal, 14, 0);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_modal, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_modal, 8, 0);

    lv_obj_t *title = lv_label_create(s_modal);
    lv_label_set_text(title, param_label(id));
    lv_obj_set_style_text_font(title, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(title, UI_IOS_TEXT_2, 0);

    s_modal_value = lv_label_create(s_modal);
    lv_obj_set_style_text_font(s_modal_value, &lv_font_cyr_24, 0);
    lv_obj_set_style_text_color(s_modal_value, UI_IOS_TEXT, 0);
    modal_update_value();

    lv_obj_t *row = lv_obj_create(s_modal);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 50);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *minus = ui_ios_btn_create(row, "−", UI_IOS_CARD_2);
    lv_obj_set_width(minus, 72);
    lv_obj_add_event_cb(minus, modal_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

    lv_obj_t *plus = ui_ios_btn_create(row, "+", UI_IOS_CARD_2);
    lv_obj_set_width(plus, 72);
    lv_obj_add_event_cb(plus, modal_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *ok = ui_ios_btn_create(row, "OK", UI_IOS_ACCENT);
    lv_obj_set_width(ok, 96);
    lv_obj_add_event_cb(ok, modal_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel = ui_ios_btn_create(s_modal, "Отмена", UI_IOS_CARD_2);
    lv_obj_set_width(cancel, lv_pct(100));
    lv_obj_add_event_cb(cancel, modal_cancel_cb, LV_EVENT_CLICKED, NULL);
}

bool ui_screen_ssvc_modal_active(void) {
    return s_modal != NULL;
}

bool ui_screen_ssvc_modal_rotate(int dir) {
    if (!s_modal || dir == 0) return false;
    s_modal_edit += (float)dir * param_step(s_modal_param);
    if (s_modal_edit < param_min(s_modal_param)) s_modal_edit = param_min(s_modal_param);
    if (s_modal_edit > param_max(s_modal_param)) s_modal_edit = param_max(s_modal_param);
    modal_update_value();
    return true;
}

bool ui_screen_ssvc_modal_press(void) {
    if (!s_modal) return false;
    ssvc_set_param(param_name(s_modal_param), s_modal_edit);
    modal_close();
    ui_toast("Параметр сохранён");
    return true;
}

static void param_click_cb(lv_event_t *event) {
    open_edit_modal((ssvc_param_id_t)(intptr_t)lv_event_get_user_data(event));
}

// ------------------------------------------------------------------
// Экран
// ------------------------------------------------------------------
static const char *stage_ru(ssvc_stage_t stage) {
    switch (stage) {
    case SSVC_STAGE_HEADS: return "ГОЛОВЫ";
    case SSVC_STAGE_BODY:  return "ТЕЛО";
    case SSVC_STAGE_TAILS: return "ХВОСТЫ";
    case SSVC_STAGE_DONE:  return "ГОТОВО";
    default:               return "—";
    }
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       lv_color_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

static lv_obj_t *row_create(lv_obj_t *parent) {
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
    lv_obj_set_style_pad_column(row, 14, 0);
    return row;
}

static lv_obj_t *stat_block(lv_obj_t *parent, const char *caption,
                            lv_color_t value_color, bool clickable,
                            ssvc_param_id_t id) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 1, 0);

    label(box, caption, &lv_font_cyr_16, UI_IOS_TEXT_2);
    lv_obj_t *value = label(box, "—", &lv_font_cyr_24, value_color);

    if (clickable) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(box, param_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)id);
    }
    return value;
}

static void pulse_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_valve_dot) return;
    static bool big = false;
    bool open = ssvc_is_valve_open();
    if (!open) {
        big = false;
        lv_obj_set_size(s_valve_dot, 12, 12);
        return;
    }
    big = !big;
    lv_obj_set_size(s_valve_dot, big ? 16 : 12, big ? 16 : 12);
}

static void update_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_stage) return;

    ssvc_state_t st;
    ssvc_get_state(&st);

    lv_label_set_text(s_stage, stage_ru(st.stage));

    char buf[48];
    if (st.t_fix > 0.0f) snprintf(buf, sizeof(buf), "%.2f°", (double)st.t_fix);
    else snprintf(buf, sizeof(buf), "—");
    lv_label_set_text(s_tfix, buf);

    snprintf(buf, sizeof(buf), "%.2f с", (double)st.open_time_sec);
    lv_label_set_text(s_open, buf);

    snprintf(buf, sizeof(buf), "%.2f с", (double)st.close_time_sec);
    lv_label_set_text(s_close, buf);

    if (st.countdown_sec > 0.0f)
        snprintf(buf, sizeof(buf), "%.2f с", (double)st.countdown_sec);
    else
        snprintf(buf, sizeof(buf), "—");
    lv_label_set_text(s_countdown, buf);

    if (st.ethanol_remaining_ml > 0.0f) {
        snprintf(buf, sizeof(buf), "%.0f мл · %.0f%%",
                 (double)st.ethanol_remaining_ml,
                 (double)st.ethanol_percent);
    } else {
        snprintf(buf, sizeof(buf), "—");
    }
    lv_label_set_text(s_ethanol, buf);

    lv_label_set_text(s_valve_text, st.valve_open ? "ОТКРЫТ" : "ЗАКРЫТ");
    lv_obj_set_style_text_color(s_valve_text,
        st.valve_open ? UI_IOS_GREEN : UI_IOS_RED, 0);
    lv_obj_set_style_bg_color(s_valve_dot,
        st.valve_open ? UI_IOS_GREEN : UI_IOS_RED, 0);

    lv_label_set_text(s_state, st.active ? "SSVC активен" : "SSVC не активен");
}

static void screen_delete_cb(lv_event_t *event) {
    (void)event;
    modal_close();
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    if (s_pulse_timer) {
        lv_timer_del(s_pulse_timer);
        s_pulse_timer = NULL;
    }
    s_stage = s_tfix = s_open = s_close = s_countdown = NULL;
    s_valve_dot = s_valve_text = s_ethanol = s_state = NULL;
}

static void next_cb(lv_event_t *event) {
    (void)event;
    ssvc_force_next_stage();
    ui_toast("Следующий этап SSVC");
}

static void back_cb(lv_event_t *event) {
    (void)event;
    ui_menu_show_root(0);
}

void ui_screen_ssvc_build(lv_obj_t *parent) {
    if (!parent) return;

    lv_obj_set_style_bg_color(parent, UI_IOS_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 8, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_set_size(back, 36, 30);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_set_style_bg_color(back, UI_IOS_CARD, 0);
    lv_obj_t *bl = label(back, LV_SYMBOL_LEFT, &lv_font_cyr_16, lv_color_white());
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);

    label(header, "SSVC0059", &lv_font_cyr_24, UI_IOS_TEXT);

    lv_obj_t *card = ui_ios_card_create(parent);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *row1 = row_create(card);
    s_stage = stat_block(row1, "ЭТАП", UI_IOS_TEXT, false, P_TFIX);

    lv_obj_t *valve_box = lv_obj_create(row1);
    lv_obj_set_flex_grow(valve_box, 1);
    lv_obj_set_height(valve_box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(valve_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(valve_box, 0, 0);
    lv_obj_set_style_pad_all(valve_box, 0, 0);
    lv_obj_clear_flag(valve_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(valve_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(valve_box, 1, 0);
    label(valve_box, "КЛАПАН", &lv_font_cyr_16, UI_IOS_TEXT_2);
    lv_obj_t *vrow = lv_obj_create(valve_box);
    lv_obj_set_width(vrow, LV_SIZE_CONTENT);
    lv_obj_set_height(vrow, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(vrow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vrow, 0, 0);
    lv_obj_set_style_pad_all(vrow, 0, 0);
    lv_obj_clear_flag(vrow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(vrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(vrow, 8, 0);

    s_valve_dot = lv_obj_create(vrow);
    lv_obj_set_size(s_valve_dot, 12, 12);
    lv_obj_set_style_radius(s_valve_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_valve_dot, UI_IOS_RED, 0);
    lv_obj_set_style_bg_opa(s_valve_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_valve_dot, 0, 0);
    lv_obj_clear_flag(s_valve_dot, LV_OBJ_FLAG_SCROLLABLE);

    s_valve_text = label(vrow, "—", &lv_font_cyr_24, UI_IOS_TEXT_2);

    lv_obj_t *row2 = row_create(card);
    s_tfix = stat_block(row2, "T ФИКСАЦИИ", UI_IOS_TEXT, true, P_TFIX);
    s_countdown = stat_block(row2, "ДО ЗАКРЫТИЯ", UI_IOS_TEXT, false, P_TFIX);

    lv_obj_t *row3 = row_create(card);
    s_open = stat_block(row3, "ОТКРЫТ", UI_IOS_TEXT, true, P_OPEN);
    s_close = stat_block(row3, "ЗАКРЫТ", UI_IOS_TEXT, true, P_CLOSE);

    s_ethanol = stat_block(card, "ОСТАТОК СПИРТА", UI_IOS_ACCENT, false, P_TFIX);
    lv_obj_set_flex_grow(lv_obj_get_parent(s_ethanol), 0);
    lv_obj_set_width(lv_obj_get_parent(s_ethanol), lv_pct(100));

    s_state = label(parent, "SSVC не активен", &lv_font_cyr_16, UI_IOS_TEXT_2);

    lv_obj_t *btn = ui_ios_btn_create(parent, "СЛЕД. ЭТАП", UI_IOS_ACCENT);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_add_event_cb(btn, next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(parent, screen_delete_cb, LV_EVENT_DELETE, NULL);
    s_timer = lv_timer_create(update_cb, 500, NULL);
    s_pulse_timer = lv_timer_create(pulse_cb, 500, NULL);
    update_cb(NULL);
}
