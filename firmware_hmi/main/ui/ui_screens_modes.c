/*
 * Экраны остальных режимов: Подготовка / Диагностика / Система.
 * Стиль тот же, что у Главного меню и Ректификации.
 */

#include "ui_screens_modes.h"
#include "ui_fonts.h"
#include "ui_futuristic.h"
#include "ui_metrics.h"
#include <stdio.h>

static lv_color_t c_white, c_muted, c_cyan, c_green, c_yellow, c_orange;

/* --- подготовка --- */
static lv_obj_t *p_scr, *p_profile, *p_t1, *p_t2, *p_level, *p_flow;
static lv_obj_t *p_rinse, *p_sealed, *p_preheat;

/* --- диагностика --- */
static lv_obj_t *d_scr, *d_can, *d_crc, *d_frames, *d_latency;
static lv_obj_t *d_heap, *d_valve, *d_pump, *d_mixer, *d_heater;

/* --- система --- */
static lv_obj_t *s_scr, *s_version, *s_available, *s_api, *s_uptime;
static lv_obj_t *s_cpu, *s_psram, *s_progress;

static void init_colors(void)
{
    c_white  = lv_color_hex(UI_WHITE);
    c_muted  = lv_color_hex(UI_MUTED_BLUE);
    c_cyan   = lv_color_hex(UI_NEON_CYAN);
    c_green  = lv_color_hex(UI_BRIGHT_GREEN);
    c_yellow = lv_color_hex(UI_WARN_YELLOW);
    c_orange = lv_color_hex(UI_WARN_ORANGE);
}

static lv_obj_t *make_root(const char *title)
{
    init_colors();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 800, 480);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_style_pad_gap(scr, 10, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050A14), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    ui_fut_header(scr, title, "ЭТАП: ТЕЛО");
    return scr;
}

static lv_obj_t *make_body(lv_obj_t *root)
{
    lv_obj_t *b = lv_obj_create(root);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_pad_gap(b, 10, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    return b;
}

static void add_footer(lv_obj_t *root)
{
    lv_obj_t *f = lv_obj_create(root);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, 24);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(f, 0, 0);
    lv_obj_set_style_pad_all(f, 0, 0);
    lv_obj_t *l = ui_fut_label(f,
        "● ВХОД  •  УДЕРЖ. 2с — НАЗАД  •  ЭНКОДЕР — НАВИГАЦИЯ",
        &cyr_14, c_muted);
    lv_obj_center(l);
}

static lv_obj_t *make_card(lv_obj_t *parent, const char *cap)
{
    lv_obj_t *c = ui_fut_panel(parent);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_height(c, LV_PCT(100));
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_set_style_pad_gap(c, 6, 0);
    ui_fut_label(c, cap, &cyr_16, c_muted);
    return c;
}

static lv_obj_t *make_lrow(lv_obj_t *parent, const char *name,
                           const char *value, lv_color_t color, bool dot)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, 34);
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
    lv_obj_set_style_pad_gap(right, 8, 0);
    if (dot) ui_fut_dot(right, color, 10);
    return ui_fut_label(right, value, &cyr_16, color);
}

static lv_obj_t *make_tile(lv_obj_t *parent, const char *cap,
                           const char *val, lv_color_t color)
{
    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_height(t, 62);
    lv_obj_set_style_radius(t, 10, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(0x0A1830), 0);
    lv_obj_set_style_border_color(t, lv_color_hex(0x254363), 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_pad_all(t, 8, 0);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    ui_fut_label(t, cap, &cyr_12, c_muted);
    return ui_fut_label(t, val, &cyr_20, color);
}

static lv_obj_t *make_action(lv_obj_t *parent, const char *text, bool primary)
{
    lv_obj_t *b = ui_fut_panel(parent);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_height(b, 34);
    lv_obj_set_style_pad_all(b, 4, 0);
    lv_obj_set_style_radius(b, 10, 0);
    if (primary) {
        lv_obj_set_style_border_color(b, c_cyan, 0);
        lv_obj_set_style_shadow_color(b, c_cyan, 0);
        lv_obj_set_style_shadow_width(b, 14, 0);
        lv_obj_set_style_shadow_opa(b, LV_OPA_50, 0);
    }
    lv_obj_t *l = ui_fut_label(b, text, &cyr_14,
                               primary ? c_white : lv_color_hex(0xD9F0FF));
    lv_obj_center(l);
    return b;
}

/* ================= ПОДГОТОВКА ================= */

lv_obj_t *ui_screen_preparation_create(void)
{
    p_scr = make_root("ПОДГОТОВКА");
    lv_obj_t *body = make_body(p_scr);

    lv_obj_t *top = lv_obj_create(body);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_height(top, 190);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_pad_gap(top, 10, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);

    lv_obj_t *c1 = make_card(top, "ПРОФИЛИ");
    p_profile = make_lrow(c1, "ВИНОГРАД · БЕЛЫЙ", "АКТИВЕН", c_cyan, true);
    make_lrow(c1, "ЗЕРНО · БУРБОН", "Выбрать ›", c_white, false);
    make_lrow(c1, "ТРАВЫ · ДЖИН", "Выбрать ›", c_white, false);
    make_lrow(c1, "СВОЙ ПРОФИЛЬ", "Выбрать ›", c_white, false);

    lv_obj_t *c2 = make_card(top, "КАЛИБРОВКА");
    p_t1   = make_lrow(c2, "ДАТЧИК T1", "78.50°", c_yellow, false);
    p_t2   = make_lrow(c2, "ДАТЧИК T2", "79.10°", c_white, false);
    p_level= make_lrow(c2, "УРОВЕНЬ", "72%", c_white, false);
    p_flow = make_lrow(c2, "ПОТОК", "12.6", c_white, false);

    lv_obj_t *wide = ui_fut_panel(body);
    lv_obj_set_width(wide, LV_PCT(100));
    lv_obj_set_flex_grow(wide, 1);
    lv_obj_set_flex_flow(wide, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(wide, 12, 0);
    lv_obj_set_style_pad_gap(wide, 16, 0);

    lv_obj_t *left = lv_obj_create(wide);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    ui_fut_label(left, "ПРЕДПУСКОВАЯ ПРОВЕРКА",
                 &cyr_16, c_muted);
    lv_obj_t *tiles = lv_obj_create(left);
    lv_obj_set_width(tiles, LV_PCT(100));
    lv_obj_set_flex_grow(tiles, 1);
    lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(tiles, 0, 0);
    lv_obj_set_style_pad_gap(tiles, 10, 0);
    lv_obj_set_style_bg_opa(tiles, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tiles, 0, 0);
    p_rinse   = make_tile(tiles, "ПРОМЫВКА", "ГОТОВО", c_green);
    p_preheat = make_tile(tiles, "НАГРЕВ", "24°", c_yellow);
    p_sealed  = make_tile(tiles, "ГЕРМЕТИЧНОСТЬ", "OK", c_green);

    lv_obj_t *btns = lv_obj_create(wide);
    lv_obj_set_width(btns, 190);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(btns, 8, 0);
    make_action(btns, "ПРОМЫТЬ", false);
    make_action(btns, "ПРОГРЕТЬ", false);
    make_action(btns, "НАЧАТЬ ПОДГОТОВКУ", true);

    add_footer(p_scr);
    lv_scr_load(p_scr);
    return p_scr;
}

void ui_screen_preparation_update(const prep_state_t *s)
{
    if (!s || !p_scr) return;
    lv_label_set_text(p_profile, s->profile ? s->profile : "—");
    lv_label_set_text_fmt(p_t1, "%.2f°", s->t1);
    lv_label_set_text_fmt(p_t2, "%.2f°", s->t2);
    lv_label_set_text_fmt(p_level, "%u%%", s->level);
    lv_label_set_text_fmt(p_flow, "%.1f", s->flow);
    lv_label_set_text(p_rinse, s->rinse_done ? "ГОТОВО" : "НЕТ");
    lv_label_set_text_fmt(p_preheat, "%.0f°", s->preheat);
    lv_label_set_text(p_sealed, s->sealed ? "OK" : "FAULT");
}

/* ================= ДИАГНОСТИКА ================= */

lv_obj_t *ui_screen_diagnostics_create(void)
{
    d_scr = make_root("ДИАГНОСТИКА");
    lv_obj_t *body = make_body(d_scr);

    lv_obj_t *top = lv_obj_create(body);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_height(top, 190);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_pad_gap(top, 10, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);

    lv_obj_t *c1 = make_card(top, "ЖУРНАЛ");
    make_lrow(c1, "15:42:08", "ПОЛЛ OK", c_green, false);
    make_lrow(c1, "15:41:55", "T ФИКС", c_yellow, false);
    make_lrow(c1, "15:40:12", "СТАРТ", c_white, false);
    make_lrow(c1, "15:38:47", "ПРОФИЛЬ", c_white, false);

    lv_obj_t *c2 = make_card(top, "ПРИВОДЫ");
    d_valve = make_lrow(c2, "КЛАПАН", "ОТКРЫТ", c_green, true);
    d_pump  = make_lrow(c2, "НАСОС", "ВКЛ", c_green, false);
    d_mixer = make_lrow(c2, "МЕШАЛКА", "42%", c_white, false);
    d_heater= make_lrow(c2, "ТЭН", "64%", c_yellow, false);

    lv_obj_t *c3 = make_card(top, "СЕТЬ");
    d_can  = make_lrow(c3, "CAN", "OK", c_green, true);
    make_lrow(c3, "HMI", "MASTER", c_cyan, false);
    d_crc    = make_lrow(c3, "RX ERR", "0", c_white, false);
    make_lrow(c3, "ПОЛЛ", "1 Гц", c_white, false);

    lv_obj_t *wide = ui_fut_panel(body);
    lv_obj_set_width(wide, LV_PCT(100));
    lv_obj_set_flex_grow(wide, 1);
    lv_obj_set_flex_flow(wide, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(wide, 12, 0);
    lv_obj_set_style_pad_gap(wide, 16, 0);

    lv_obj_t *left = lv_obj_create(wide);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    ui_fut_label(left, "ОТЧЕТЫ И ТЕСТЫ", &cyr_16, c_muted);
    lv_obj_t *tiles = lv_obj_create(left);
    lv_obj_set_width(tiles, LV_PCT(100));
    lv_obj_set_flex_grow(tiles, 1);
    lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(tiles, 0, 0);
    lv_obj_set_style_pad_gap(tiles, 10, 0);
    lv_obj_set_style_bg_opa(tiles, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tiles, 0, 0);
    d_latency = make_tile(tiles, "ЗАДЕРЖКА", "12 мс", c_green);
    d_frames  = make_tile(tiles, "КАДРОВ", "18402", c_white);
    d_heap    = make_tile(tiles, "HEAP", "188 КБ", c_white);

    lv_obj_t *btns = lv_obj_create(wide);
    lv_obj_set_width(btns, 190);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(btns, 8, 0);
    make_action(btns, "ЭКСПОРТ ЛОГА", false);
    make_action(btns, "ТЕСТ ПРИВОДОВ", false);
    make_action(btns, "ПИНГ КОНТРОЛЛЕРА", true);

    add_footer(d_scr);
    lv_scr_load(d_scr);
    return d_scr;
}

void ui_screen_diagnostics_update(const diag_state_t *s)
{
    if (!s || !d_scr) return;
    lv_label_set_text(d_can, s->can_ok ? "OK" : "NO LINK");
    lv_obj_set_style_text_color(d_can,
        s->can_ok ? c_green : c_orange, 0);
    lv_label_set_text_fmt(d_crc, "%u", s->rx_err);
    lv_label_set_text_fmt(d_frames, "%u", s->frames);
    lv_label_set_text_fmt(d_latency, "%u мс", s->latency_ms);
    lv_label_set_text_fmt(d_heap, "%u КБ", s->heap_kb);
    lv_label_set_text(d_valve, s->valve_open ? "ОТКРЫТ" : "ЗАКРЫТ");
    lv_label_set_text(d_pump, s->pump_on ? "ВКЛ" : "ВЫКЛ");
    lv_label_set_text_fmt(d_mixer, "%u%%", s->mixer);
    lv_label_set_text_fmt(d_heater, "%u%%", s->heater);
}

/* ================= СИСТЕМА ================= */

lv_obj_t *ui_screen_system_create(void)
{
    s_scr = make_root("СИСТЕМА");
    lv_obj_t *body = make_body(s_scr);

    lv_obj_t *top = lv_obj_create(body);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_height(top, 190);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_pad_gap(top, 10, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);

    lv_obj_t *c1 = make_card(top, "ОБНОВЛЕНИЕ");
    s_version   = make_lrow(c1, "ТЕКУЩАЯ", "v0.9.6-hmi1", c_white, false);
    s_available = make_lrow(c1, "ДОСТУПНА", "v0.9.7-hmi1", c_yellow, false);
    make_lrow(c1, "СТАТУС", "ОЖИДАНИЕ", c_green, false);
    s_progress = lv_bar_create(c1);
    lv_obj_set_width(s_progress, LV_PCT(100));
    lv_obj_set_height(s_progress, 10);
    lv_bar_set_range(s_progress, 0, 100);
    lv_bar_set_value(s_progress, 0, LV_ANIM_OFF);

    lv_obj_t *c2 = make_card(top, "API");
    s_api = make_lrow(c2, "GET /api/state", "OK", c_green, false);
    make_lrow(c2, "POST /api/start", "OK", c_green, false);
    make_lrow(c2, "ТОКЕН", "••••••••", c_white, false);
    make_lrow(c2, "LIMIT", "10 req/s", c_white, false);

    lv_obj_t *c3 = make_card(top, "ЯЗЫК");
    make_lrow(c3, "РУССКИЙ", "АКТИВЕН", c_cyan, true);
    make_lrow(c3, "ENGLISH", "Выбрать ›", c_white, false);
    make_lrow(c3, "ФОРМАТ ДАТЫ", "ДД.ММ.ГГ", c_white, false);
    make_lrow(c3, "ЕД. ИЗМ.", "°C / мл", c_white, false);

    lv_obj_t *wide = ui_fut_panel(body);
    lv_obj_set_width(wide, LV_PCT(100));
    lv_obj_set_flex_grow(wide, 1);
    lv_obj_set_flex_flow(wide, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(wide, 12, 0);
    lv_obj_set_style_pad_gap(wide, 16, 0);

    lv_obj_t *left = lv_obj_create(wide);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    ui_fut_label(left, "СЕРВИС", &cyr_16, c_muted);
    lv_obj_t *tiles = lv_obj_create(left);
    lv_obj_set_width(tiles, LV_PCT(100));
    lv_obj_set_flex_grow(tiles, 1);
    lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(tiles, 0, 0);
    lv_obj_set_style_pad_gap(tiles, 10, 0);
    lv_obj_set_style_bg_opa(tiles, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tiles, 0, 0);
    s_uptime = make_tile(tiles, "UPTIME", "142 ч", c_white);
    s_cpu    = make_tile(tiles, "CPU TEMP", "48°", c_yellow);
    s_psram  = make_tile(tiles, "PSRAM", "4.2 МБ", c_white);

    lv_obj_t *btns = lv_obj_create(wide);
    lv_obj_set_width(btns, 190);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(btns, 8, 0);
    make_action(btns, "ПРОВЕРИТЬ ОБНОВЛЕНИЕ", false);
    make_action(btns, "BACKUP КОНФИГА", false);
    make_action(btns, "ПЕРЕЗАГРУЗКА HMI", true);

    add_footer(s_scr);
    lv_scr_load(s_scr);
    return s_scr;
}

void ui_screen_system_update(const sys_state_t *s)
{
    if (!s || !s_scr) return;
    lv_label_set_text(s_version, s->version ? s->version : "—");
    lv_label_set_text(s_available, s->available ? s->available : "—");
    lv_label_set_text(s_api, s->api_ok ? "OK" : "FAULT");
    lv_obj_set_style_text_color(s_api, s->api_ok ? c_green : c_orange, 0);
    lv_label_set_text_fmt(s_uptime, "%u ч", s->uptime_h);
    lv_label_set_text_fmt(s_cpu, "%u°", s->cpu_temp);
    lv_label_set_text_fmt(s_psram, "%u.%u МБ", s->psram_mb / 10, s->psram_mb % 10);
    lv_bar_set_value(s_progress, s->update_progress, LV_ANIM_OFF);
}
