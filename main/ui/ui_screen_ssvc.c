#include "ui_screen_ssvc.h"
#include "ui.h"
#include "ui_menu.h"
#include "ui_ios_theme.h"

#include "control/fraction_ssvc.h"

#include <stdio.h>

static lv_obj_t *s_stage;
static lv_obj_t *s_tfix;
static lv_obj_t *s_open;
static lv_obj_t *s_valve;
static lv_obj_t *s_state;
static lv_timer_t *s_timer;

static const char *stage_ru(ssvc_stage_t stage) {
    switch (stage) {
    case SSVC_STAGE_HEADS: return "ГОЛОВЫ";
    case SSVC_STAGE_BODY:  return "ТЕЛО";
    case SSVC_STAGE_TAILS: return "ХВОСТЫ";
    case SSVC_STAGE_DONE:  return "ГОТОВО";
    default:               return "—";
    }
}

static void update_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_stage) return;

    ssvc_state_t st;
    ssvc_get_state(&st);

    lv_label_set_text(s_stage, stage_ru(st.stage));

    char buf[48];
    if (st.t_fix > 0.0f) snprintf(buf, sizeof(buf), "%.2f°", st.t_fix);
    else snprintf(buf, sizeof(buf), "—");
    lv_label_set_text(s_tfix, buf);

    snprintf(buf, sizeof(buf), "%.2f с", st.open_time_sec);
    lv_label_set_text(s_open, buf);

    lv_label_set_text(s_valve, st.valve_open ? "ОТКРЫТ" : "ЗАКРЫТ");
    lv_obj_set_style_text_color(s_valve,
        st.valve_open ? UI_IOS_GREEN : UI_IOS_TEXT_2, 0);

    lv_label_set_text(s_state, st.active ? "SSVC активен" : "SSVC не активен");
}

static void screen_delete_cb(lv_event_t *event) {
    (void)event;
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    s_stage = s_tfix = s_open = s_valve = s_state = NULL;
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

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       lv_color_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

// Компактный блок «подпись + значение» для сетки 2x2 на 480x320.
static lv_obj_t *stat_block(lv_obj_t *parent, const char *caption,
                            lv_color_t value_color) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 2, 0);

    label(box, caption, &lv_font_cyr_16, UI_IOS_TEXT_2);
    return label(box, "—", &lv_font_cyr_24, value_color);
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
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 16, 0);
    return row;
}

void ui_screen_ssvc_build(lv_obj_t *parent) {
    if (!parent) return;

    lv_obj_set_style_bg_color(parent, UI_IOS_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
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
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *row1 = row_create(card);
    s_stage = stat_block(row1, "ЭТАП", UI_IOS_TEXT);
    s_tfix = stat_block(row1, "T ФИКСАЦИИ", UI_IOS_TEXT);

    lv_obj_t *row2 = row_create(card);
    s_open = stat_block(row2, "ОТКРЫТ", UI_IOS_TEXT);
    s_valve = stat_block(row2, "КЛАПАН", UI_IOS_TEXT_2);

    s_state = label(parent, "SSVC не активен", &lv_font_cyr_16, UI_IOS_TEXT_2);

    lv_obj_t *btn = ui_ios_btn_create(parent, "СЛЕД. ЭТАП", UI_IOS_ACCENT);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_add_event_cb(btn, next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(parent, screen_delete_cb, LV_EVENT_DELETE, NULL);
    s_timer = lv_timer_create(update_cb, 500, NULL);
    update_cb(NULL);
}
