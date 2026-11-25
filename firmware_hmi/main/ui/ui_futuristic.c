#include "ui_futuristic.h"
#include "ui_fonts.h"
#include "ui_metrics.h"
#include <time.h>
#include <stdio.h>

static lv_obj_t *s_clock;
static uint32_t s_clock_epoch;
static ui_fut_overlay_cb_t s_overlay_cb;

static void overlay_event_cb(lv_event_t *e)
{
    if (s_overlay_cb) s_overlay_cb(lv_event_get_user_data(e));
}

void ui_fut_set_overlay_cb(ui_fut_overlay_cb_t cb)
{
    s_overlay_cb = cb;
}

static lv_color_t c_glass, c_glass2, c_line, c_cyan, c_blue;
static lv_color_t c_green, c_yellow, c_white, c_muted;

static void ensure_colors(void)
{
    c_glass  = lv_color_hex(UI_GLASS);
    c_glass2 = lv_color_hex(UI_GLASS_ALT);
    c_line   = lv_color_hex(0x50BFFF);
    c_cyan   = lv_color_hex(UI_NEON_CYAN);
    c_blue   = lv_color_hex(UI_NEON_BLUE);
    c_green  = lv_color_hex(UI_BRIGHT_GREEN);
    c_yellow = lv_color_hex(UI_WARN_YELLOW);
    c_white  = lv_color_hex(UI_WHITE);
    c_muted  = lv_color_hex(UI_MUTED_BLUE);
}

lv_obj_t *ui_fut_label(lv_obj_t *parent, const char *txt,
                       const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

lv_obj_t *ui_fut_dot(lv_obj_t *parent, lv_color_t color, int size)
{
    lv_obj_t *d = lv_obj_create(parent);
    lv_obj_set_size(d, size, size);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, color, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_pad_all(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    return d;
}

lv_obj_t *ui_fut_panel(lv_obj_t *parent)
{
    ensure_colors();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_style_radius(p, 14, 0);
    lv_obj_set_style_bg_color(p, c_glass, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_color(p, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_pad_all(p, 10, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t *ui_fut_panel_active(lv_obj_t *parent)
{
    lv_obj_t *p = ui_fut_panel(parent);
    lv_obj_set_style_bg_color(p, c_glass2, 0);
    lv_obj_set_style_border_color(p, c_cyan, 0);
    lv_obj_set_style_border_width(p, 2, 0);
    lv_obj_set_style_shadow_color(p, c_cyan, 0);
    lv_obj_set_style_shadow_width(p, 26, 0);
    lv_obj_set_style_shadow_opa(p, LV_OPA_65, 0);
    return p;
}

lv_obj_t *ui_fut_stage_pill(lv_obj_t *parent, const char *text)
{
    ensure_colors();
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_height(p, 38);
    lv_obj_set_width(p, 168);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x1B3A66), 0);
    lv_obj_set_style_border_color(p, lv_color_hex(0x78C8FF), 0);
    lv_obj_set_style_border_width(p, 2, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_pad_left(p, 18, 0);
    lv_obj_set_style_pad_right(p, 18, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = ui_fut_label(p, text, &cyr_18,
                               lv_color_hex(0xD9F0FF));
    lv_obj_center(l);
    return p;
}

lv_obj_t *ui_fut_header(lv_obj_t *parent, const char *title,
                        const char *stage)
{
    ensure_colors();
    lv_obj_t *h = ui_fut_panel(parent);
    lv_obj_set_width(h, LV_PCT(100));
    lv_obj_set_height(h, 72);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(h, 12, 0);

    lv_obj_t *left = lv_obj_create(h);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_fut_dot(left, c_green, 12);
    ui_fut_label(left, "ГОТОВ", &cyr_18, c_white);

    ui_fut_label(h, title, &cyr_28, c_white);

    lv_obj_t *right = lv_obj_create(h);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(right, 16, 0);
    ui_fut_stage_pill(right, stage ? stage : "ЭТАП: ТЕЛО");
    s_clock = ui_fut_label(right, "--:--:--",
                           &cyr_18, c_white);
    ui_fut_clock_tick();

    /* Глобальные иконки: справка и калькуляторы — на каждом экране */
    lv_obj_t *help = ui_fut_label(right, "?", &cyr_18, c_cyan);
    lv_obj_add_flag(help, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(help, overlay_event_cb, LV_EVENT_CLICKED,
                        UI_FUT_OVERLAY_HELP);

    lv_obj_t *calc = ui_fut_label(right, "fx", &cyr_18, c_cyan);
    lv_obj_add_flag(calc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(calc, overlay_event_cb, LV_EVENT_CLICKED,
                        UI_FUT_OVERLAY_CALC);

    ui_fut_label(right, LV_SYMBOL_WIFI, &lv_font_montserrat_18, c_cyan);
    ui_fut_label(right, "°C", &cyr_18, c_cyan);

    return h;
}

lv_obj_t *ui_fut_menu_button(lv_obj_t *parent, const char *icon,
                             const char *title, const char *subtitle,
                             bool active, lv_event_cb_t cb, void *user_data)
{
    ensure_colors();
    lv_obj_t *b = active ? ui_fut_panel_active(parent) : ui_fut_panel(parent);
    lv_obj_set_height(b, 140);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(b, 14, 0);
    lv_obj_set_style_pad_gap(b, 12, 0);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    }

    /* Крупная иконка без квадратной рамки */
    lv_obj_t *ic = ui_fut_label(b, icon, &lv_font_montserrat_32,
                                lv_color_hex(0xA9DCFF));
    lv_obj_set_width(ic, 64);
    lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *txt = lv_obj_create(b);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_height(txt, LV_PCT(100));
    lv_obj_set_style_bg_opa(txt, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(txt, 0, 0);
    lv_obj_set_style_pad_all(txt, 0, 0);
    lv_obj_set_style_pad_gap(txt, 10, 0);
    lv_obj_set_flex_flow(txt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(txt, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    ui_fut_label(txt, title, &cyr_24, c_white);
    ui_fut_label(txt, subtitle, &cyr_16, c_muted);

    ui_fut_label(b, LV_SYMBOL_RIGHT, &lv_font_montserrat_28, c_white);
    return b;
}

void ui_fut_clock_set_epoch(uint32_t epoch)
{
    s_clock_epoch = epoch;
    ui_fut_clock_tick();
}

void ui_fut_clock_tick(void)
{
    if (!s_clock) return;

    time_t now = s_clock_epoch ? (time_t)s_clock_epoch : time(NULL);
    struct tm tmv;
    if (localtime_r(&now, &tmv) == NULL) {
        lv_label_set_text(s_clock, "--:--:--");
        return;
    }
    lv_label_set_text_fmt(s_clock, "%02d:%02d:%02d",
                          tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
}
