/*
 * Экран 1: "Ректификация".
 * Стиль: futuristic industrial control panel, тёмная тема, неоновый cyan.
 */

#include "ui_screen_rectification.h"
#include "ui_fonts.h"
#include "ui_futuristic.h"
#include "ui_metrics.h"
#include <stdio.h>

static lv_obj_t *s_scr;
static lv_obj_t *s_steam;
static lv_obj_t *s_steam_set;
static lv_obj_t *s_steam_fix;
static lv_obj_t *s_tfix;
static lv_obj_t *s_open;
static lv_obj_t *s_close;
static lv_obj_t *s_remain;
static lv_obj_t *s_valve;
static lv_obj_t *s_alcohol;
static lv_obj_t *s_strength;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_kub;
static lv_chart_series_t *s_par;

static lv_color_t c_white, c_muted, c_cyan, c_blue, c_green, c_yellow, c_orange;

static void init_colors(void)
{
    c_white  = lv_color_hex(UI_WHITE);
    c_muted  = lv_color_hex(UI_MUTED_BLUE);
    c_cyan   = lv_color_hex(UI_NEON_CYAN);
    c_blue   = lv_color_hex(UI_NEON_BLUE);
    c_green  = lv_color_hex(UI_BRIGHT_GREEN);
    c_yellow = lv_color_hex(UI_WARN_YELLOW);
    c_orange = lv_color_hex(UI_WARN_ORANGE);
}

static lv_obj_t *make_small_card(lv_obj_t *parent, const char *cap)
{
    lv_obj_t *c = ui_fut_panel(parent);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_height(c, LV_PCT(100));
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    ui_fut_label(c, cap, &cyr_14, c_muted);
    return c;
}

static lv_obj_t *make_row(lv_obj_t *parent, const char *name,
                          const lv_font_t *font, lv_color_t color,
                          bool editable)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, 32);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

    ui_fut_label(r, name, &cyr_16, c_muted);

    lv_obj_t *right = lv_obj_create(r);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(right, 7, 0);

    lv_obj_t *v = ui_fut_label(right, "—", font, color);
    if (editable)
        ui_fut_label(right, LV_SYMBOL_EDIT, &lv_font_montserrat_14, c_cyan);
    return v;
}

static void make_dashed_threshold(lv_obj_t *parent, int y,
                                  lv_color_t color, lv_opa_t opa)
{
    for (int x = 14; x < 760; x += 26) {
        lv_obj_t *seg = lv_obj_create(parent);
        lv_obj_add_flag(seg, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_size(seg, 14, 1);
        lv_obj_set_pos(seg, x, y);
        lv_obj_set_style_bg_color(seg, color, 0);
        lv_obj_set_style_bg_opa(seg, opa, 0);
        lv_obj_set_style_border_width(seg, 0, 0);
        lv_obj_set_style_pad_all(seg, 0, 0);
    }
}

lv_obj_t *ui_screen_rectification_create(void)
{
    init_colors();

    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, 800, 480);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(s_scr, 12, 0);
    lv_obj_set_style_pad_gap(s_scr, 10, 0);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x050A14), 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_fut_header(s_scr, "РЕКТИФИКАЦИЯ", "ЭТАП: ТЕЛО");

    /* --- три карточки --- */
    lv_obj_t *cards = lv_obj_create(s_scr);
    lv_obj_set_width(cards, LV_PCT(100));
    lv_obj_set_height(cards, 186);
    lv_obj_set_flex_flow(cards, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cards, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(cards, 0, 0);
    lv_obj_set_style_pad_gap(cards, 10, 0);
    lv_obj_set_style_bg_opa(cards, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cards, 0, 0);

    /* ПАР */
    lv_obj_t *c1 = make_small_card(cards, "ПАР");
    s_steam = ui_fut_label(c1, "78.3°", &cyr_32, c_white);
    lv_obj_set_style_text_font(s_steam, &cyr_32, 0);
    lv_obj_align(s_steam, LV_ALIGN_CENTER, 0, -8);
    /* Подписи УСТ и T ФИКС — друг под другом, по центру, внизу карточки */
    lv_obj_t *f1 = lv_obj_create(c1);
    lv_obj_add_flag(f1, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_width(f1, LV_PCT(100));
    lv_obj_align(f1, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_opa(f1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(f1, 0, 0);
    lv_obj_set_style_pad_all(f1, 0, 0);
    lv_obj_set_style_pad_gap(f1, 5, 0);
    lv_obj_set_flex_flow(f1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(f1, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_steam_set = ui_fut_label(f1, "УСТ 80.0°", &cyr_16, c_muted);
    s_steam_fix = ui_fut_label(f1, "T ФИКС 78.50°",
                               &cyr_16, c_yellow);

    /* ОТБОР */
    lv_obj_t *c2 = make_small_card(cards, "ОТБОР");
    s_tfix   = make_row(c2, "T ФИКСАЦИИ", &cyr_16, c_yellow, true);
    s_open   = make_row(c2, "ВРЕМЯ ОТКРЫТ", &cyr_16, c_white, true);
    s_remain = make_row(c2, "до закрытия", &cyr_16, c_muted, false);
    s_close  = make_row(c2, "ВРЕМЯ ЗАКРЫТ", &cyr_16, c_white, true);

    /* СОСТОЯНИЕ */
    lv_obj_t *c3 = make_small_card(cards, "СОСТОЯНИЕ");
    s_valve   = make_row(c3, "КЛАПАН", &cyr_16, c_green, false);
    s_alcohol = make_row(c3, "ОСТАТОК СПИРТА", &cyr_16, c_white, false);
    s_strength= make_row(c3, "КРЕПОСТЬ", &cyr_16, c_white, false);

    /* --- график --- */
    lv_obj_t *plot_panel = ui_fut_panel(s_scr);
    lv_obj_set_width(plot_panel, LV_PCT(100));
    lv_obj_set_flex_grow(plot_panel, 1);
    lv_obj_set_flex_flow(plot_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(plot_panel, 10, 0);
    lv_obj_set_style_pad_gap(plot_panel, 4, 0);

    lv_obj_t *ph = lv_obj_create(plot_panel);
    lv_obj_set_width(ph, LV_PCT(100));
    lv_obj_set_height(ph, 22);
    lv_obj_set_flex_flow(ph, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ph, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(ph, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ph, 0, 0);
    lv_obj_set_style_pad_all(ph, 0, 0);
    ui_fut_label(ph, "ТЕМПЕРАТУРА · 5 МИН",
                 &cyr_14, c_muted);
    lv_obj_t *legend = lv_obj_create(ph);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(legend, 12, 0);
    ui_fut_label(legend, "КУБ", &cyr_12, c_blue);
    ui_fut_label(legend, "ПАР", &cyr_12, c_orange);

    s_chart = lv_chart_create(plot_panel);
    lv_obj_set_width(s_chart, LV_PCT(100));
    lv_obj_set_flex_grow(s_chart, 1);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 16);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 60, 90);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(s_chart, 3, 4);
    lv_obj_set_style_pad_all(s_chart, 6, 0);
    lv_obj_set_style_line_width(s_chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    /* заметная, но тонкая сетка */
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x24405F), LV_PART_MAIN);
    lv_obj_set_style_line_opa(s_chart, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_chart, 1, LV_PART_MAIN);

    s_kub = lv_chart_add_series(s_chart, c_blue, LV_CHART_AXIS_PRIMARY_Y);
    s_par = lv_chart_add_series(s_chart, c_orange, LV_CHART_AXIS_PRIMARY_Y);

    static const int16_t kub[] = {61,63,67,71,74,76,77,78,78,78,78,78,78,78,78,78};
    static const int16_t par[] = {63,65,69,73,76,77,78,78,78,78,78,78,78,78,78,78};
    for (int i = 0; i < 16; i++) {
        lv_chart_set_value_by_id(s_chart, s_kub, i, kub[i]);
        lv_chart_set_value_by_id(s_chart, s_par, i, par[i]);
    }
    lv_chart_refresh(s_chart);

    /* Пунктирные пороги T и T ФИКС */
    make_dashed_threshold(plot_panel, 46, c_yellow, LV_OPA_70);
    make_dashed_threshold(plot_panel, 53, c_yellow, LV_OPA_40);

    lv_obj_t *axis = lv_obj_create(plot_panel);
    lv_obj_set_width(axis, LV_PCT(100));
    lv_obj_set_height(axis, 16);
    lv_obj_set_flex_flow(axis, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(axis, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(axis, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(axis, 0, 0);
    lv_obj_set_style_pad_all(axis, 0, 0);
    ui_fut_label(axis, "-5M", &cyr_12, c_muted);
    ui_fut_label(axis, "-3M", &cyr_12, c_muted);
    ui_fut_label(axis, "-1M", &cyr_12, c_muted);
    ui_fut_label(axis, "сейчас", &cyr_12, c_muted);

    lv_scr_load(s_scr);
    return s_scr;
}

void ui_screen_rectification_update(const rect_state_t *s)
{
    if (!s || !s_scr) return;

    lv_label_set_text_fmt(s_steam, "%.1f°", s->steam);
    lv_label_set_text_fmt(s_steam_set, "УСТ %.1f°", s->steam_set);
    lv_label_set_text_fmt(s_steam_fix, "T ФИКС %.2f°", s->steam_fix);

    lv_label_set_text_fmt(s_tfix, "%.2f°", s->t_fix);
    lv_label_set_text_fmt(s_open, "%.2f с", s->open_s);
    lv_label_set_text_fmt(s_remain, "%.2f с", s->remain_s);
    lv_label_set_text_fmt(s_close, "%.2f с", s->close_s);

    lv_label_set_text(s_valve, s->valve_open ? "ОТКРЫТ" : "ЗАКРЫТ");
    lv_obj_set_style_text_color(s_valve,
                                s->valve_open ? c_green : c_orange, 0);
    lv_label_set_text_fmt(s_alcohol, "%u мл", s->alcohol_ml);
    lv_label_set_text_fmt(s_strength, "%u%%", s->strength);

    lv_chart_set_next_value(s_chart, s_kub, (int16_t)s->kub);
    lv_chart_set_next_value(s_chart, s_par, (int16_t)s->par);
    lv_chart_refresh(s_chart);
}
