/*
 * Гибридный главный экран HMI для 800x480.
 *
 *   - безопасные элементы (E-STOP, KPI, кнопки) — масштаб 1.5
 *   - карточки/заголовки                    — масштаб 1.35
 *   - лог и вторичная диагностика            — масштаб 1.2
 *
 * Раскладка:
 * ┌──────────────────────────────────┬────────────┐
 * │ Header 84 px                     │ STATUS     │
 * ├──────────────┬──────────────┬─────┤ 180 px     │
 * │ Card 1       │ Card 2       │Card3│            │
 * ├──────────────┴──────────────┴─────┤            │
 * │ KPI + power slider (1.5)         │            │
 * ├──────────────────────────────────┤            │
 * │ Event strip 77 px (1.2)          │            │
 * └──────────────────────────────────┴────────────┘
 */

#include "ui_screen_home.h"
#include "ui_fonts.h"
#include "ui_metrics.h"
#include <stdio.h>

static lv_obj_t *s_scr;
static lv_obj_t *s_tc;
static lv_obj_t *s_ts;
static lv_obj_t *s_p;
static lv_obj_t *s_power_bar;
static lv_obj_t *s_power_txt;
static lv_obj_t *s_baro;
static lv_obj_t *s_flow;
static lv_obj_t *s_frac;
static lv_obj_t *s_link;
static lv_obj_t *s_es;
static lv_obj_t *s_safety;
static lv_obj_t *s_level;
static lv_obj_t *s_stage;
static lv_obj_t *s_ver;
static lv_obj_t *s_crc;
static lv_obj_t *s_ev[3];

static lv_color_t c_bg;
static lv_color_t c_card;
static lv_color_t c_text;
static lv_color_t c_muted;
static lv_color_t c_blue;
static lv_color_t c_green;
static lv_color_t c_red;
static lv_color_t c_amber;

/* ---------- мелкие помощники ---------- */

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt,
                            const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

static lv_obj_t *make_card(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_height(c, LV_PCT(100));
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(c, UI_PAD_MD, 0);
    lv_obj_set_style_radius(c, UI_CARD_R, 0);
    lv_obj_set_style_bg_color(c, c_card, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0x2A3555), 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t *make_status_row(lv_obj_t *parent, const char *name,
                                 lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, UI_PX_DENSE(28));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, UI_PAD_SM, 0);
    lv_obj_set_style_radius(row, UI_ROW_R, 0);
    lv_obj_set_style_bg_color(row, color, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_20, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    make_label(row, name, &cyr_14, c_muted);
    lv_obj_t *val = make_label(row, "—", font, c_text);
    return val;
}

/* ---------- создание экрана ---------- */

lv_obj_t *ui_screen_home_create(void)
{
    c_bg   = lv_color_hex(UI_BG);
    c_card = lv_color_hex(UI_CARD);
    c_text = lv_color_hex(UI_TEXT);
    c_muted= lv_color_hex(UI_MUTED);
    c_blue = lv_color_hex(UI_BLUE);
    c_green= lv_color_hex(UI_GREEN);
    c_red  = lv_color_hex(UI_RED);
    c_amber= lv_color_hex(UI_AMBER);

    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, 800, 480);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(s_scr, UI_PAD_SM, 0);
    lv_obj_set_style_bg_color(s_scr, c_bg, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_gap(s_scr, UI_PAD_SM, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header ---------------------------------------------------------- */
    lv_obj_t *hdr = lv_obj_create(s_scr);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, UI_HEADER_H);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(hdr, UI_PAD_MD, 0);
    lv_obj_set_style_radius(hdr, UI_ROW_R, 0);
    lv_obj_set_style_bg_color(hdr, c_card, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);

    make_label(hdr, "AUTOMATIKA  •  ПАНЕЛЬ HMI",
               &cyr_28, c_text);
    make_label(hdr, "800×480 / CAN HMI=MASTER",
               &cyr_18, c_muted);

    /* Content: main + status ------------------------------------------ */
    lv_obj_t *content = lv_obj_create(s_scr);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_gap(content, UI_PAD_SM, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* Main column ------------------------------------------------------ */
    lv_obj_t *main = lv_obj_create(content);
    lv_obj_set_flex_grow(main, 1);
    lv_obj_set_height(main, LV_PCT(100));
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(main, 0, 0);
    lv_obj_set_style_pad_gap(main, UI_PAD_SM, 0);
    lv_obj_set_style_bg_opa(main, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main, 0, 0);
    lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);

    /* Cards row (3 cards) --------------------------------------------- */
    lv_obj_t *cards = lv_obj_create(main);
    lv_obj_set_width(cards, LV_PCT(100));
    lv_obj_set_height(cards, UI_CARDS_H);
    lv_obj_set_flex_flow(cards, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cards, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(cards, 0, 0);
    lv_obj_set_style_pad_gap(cards, UI_PAD_SM, 0);
    lv_obj_set_style_bg_opa(cards, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cards, 0, 0);

    lv_obj_t *c1 = make_card(cards);
    make_label(c1, "T1 · ТЕМПЕРАТУРА", &cyr_18, c_muted);
    s_tc = make_label(c1, "0.0", &cyr_32, c_text);
    make_label(c1, "°C", &cyr_18, c_blue);

    lv_obj_t *c2 = make_card(cards);
    make_label(c2, "T2 · УСТАВКА", &cyr_18, c_muted);
    s_ts = make_label(c2, "0.0", &cyr_32, c_text);
    make_label(c2, "°C", &cyr_18, c_blue);

    lv_obj_t *c3 = make_card(cards);
    make_label(c3, "МОЩНОСТЬ", &cyr_18, c_muted);
    s_p = make_label(c3, "0%", &cyr_32, c_amber);
    make_label(c3, "регулятор", &cyr_18, c_muted);

    /* Power + KPI ------------------------------------------------------ */
    lv_obj_t *power = lv_obj_create(main);
    lv_obj_set_width(power, LV_PCT(100));
    lv_obj_set_height(power, UI_POWER_H);
    lv_obj_set_flex_flow(power, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(power, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(power, UI_PAD_MD, 0);
    lv_obj_set_style_radius(power, UI_ROW_R, 0);
    lv_obj_set_style_bg_color(power, c_card, 0);
    lv_obj_set_style_border_width(power, 0, 0);

    make_label(power, "SET POWER", &cyr_24, c_text);
    s_power_bar = lv_bar_create(power);
    lv_obj_set_size(s_power_bar, UI_PX_DENSE(260), UI_PX_DENSE(20));
    lv_bar_set_range(s_power_bar, 0, 100);
    lv_bar_set_value(s_power_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_power_bar, lv_color_hex(0x2A3555), 0);
    lv_obj_set_style_bg_color(s_power_bar, c_blue, LV_PART_INDICATOR);
    s_power_txt = make_label(power, "0%", &cyr_28, c_blue);

    /* Secondary KPI row ------------------------------------------------ */
    lv_obj_t *kpi = lv_obj_create(main);
    lv_obj_set_width(kpi, LV_PCT(100));
    lv_obj_set_flex_grow(kpi, 1);
    lv_obj_set_flex_flow(kpi, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(kpi, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(kpi, 0, 0);
    lv_obj_set_style_pad_gap(kpi, UI_PAD_SM, 0);
    lv_obj_set_style_bg_opa(kpi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kpi, 0, 0);

    lv_obj_t *k1 = make_card(kpi);
    lv_obj_set_style_pad_all(k1, UI_PAD_XS, 0);
    make_label(k1, "БАРО", &cyr_14, c_muted);
    s_baro = make_label(k1, "—", &cyr_20, c_text);

    lv_obj_t *k2 = make_card(kpi);
    lv_obj_set_style_pad_all(k2, UI_PAD_XS, 0);
    make_label(k2, "ПОТОК", &cyr_14, c_muted);
    s_flow = make_label(k2, "—", &cyr_20, c_text);

    lv_obj_t *k3 = make_card(kpi);
    lv_obj_set_style_pad_all(k3, UI_PAD_XS, 0);
    make_label(k3, "ФРАКЦИЯ", &cyr_14, c_muted);
    s_frac = make_label(k3, "—", &cyr_20, c_text);

    /* Status column ---------------------------------------------------- */
    lv_obj_t *status = lv_obj_create(content);
    lv_obj_set_width(status, UI_STATUS_W);
    lv_obj_set_height(status, LV_PCT(100));
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(status, UI_PAD_XS, 0);
    lv_obj_set_style_pad_gap(status, UI_PX_DENSE(4), 0);
    lv_obj_set_style_radius(status, UI_ROW_R, 0);
    lv_obj_set_style_bg_color(status, c_card, 0);
    lv_obj_set_style_border_width(status, 0, 0);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);

    make_label(status, "СОСТОЯНИЕ", &cyr_18, c_blue);
    s_link   = make_status_row(status, "LINK",  c_green, &cyr_18);
    s_es     = make_status_row(status, "E-STOP",c_green, &cyr_24);
    lv_obj_set_height(lv_obj_get_parent(s_es), UI_PX_DENSE(36)); // 43 px
    s_safety = make_status_row(status, "SAFETY",c_green, &cyr_18);
    s_level  = make_status_row(status, "УРОВЕНЬ", c_blue, &cyr_18);
    s_stage  = make_status_row(status, "ЭТАП",  c_blue, &cyr_18);
    s_ver    = make_status_row(status, "HMI",   c_muted, &cyr_14);
    s_crc    = make_status_row(status, "CRC ERR", c_muted, &cyr_14);

    /* Event strip ------------------------------------------------------ */
    lv_obj_t *events = lv_obj_create(s_scr);
    lv_obj_set_width(events, LV_PCT(100));
    lv_obj_set_height(events, UI_EVENTS_H);
    lv_obj_set_flex_flow(events, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(events, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(events, UI_PAD_SM, 0);
    lv_obj_set_style_pad_gap(events, UI_PAD_SM, 0);
    lv_obj_set_style_radius(events, UI_ROW_R, 0);
    lv_obj_set_style_bg_color(events, c_card, 0);
    lv_obj_set_style_border_width(events, 0, 0);
    lv_obj_clear_flag(events, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 3; i++) {
        s_ev[i] = make_label(events, "—", &cyr_18, c_muted);
        lv_obj_set_flex_grow(s_ev[i], 1);
    }

    lv_scr_load(s_scr);
    return s_scr;
}

/* ---------- обновление ---------- */

static void color_row(lv_obj_t *row, lv_color_t c)
{
    lv_obj_set_style_bg_color(row, c, 0);
}

void ui_screen_home_update(const hmi_telemetry_t *t)
{
    if (!t || !s_scr) return;

    if (t->link) {
        lv_label_set_text_fmt(s_tc, "%.1f", t->tc);
        lv_label_set_text_fmt(s_ts, "%.1f", t->ts);
        lv_label_set_text_fmt(s_p,  "%.0f%%", t->p);
        lv_label_set_text(s_power_txt, "");
        lv_label_set_text_fmt(s_power_txt, "%.0f%%", t->p);
        lv_bar_set_value(s_power_bar, (int)t->p, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_baro, "%.0f", t->baro);
        lv_label_set_text_fmt(s_flow, "%.1f", t->flow);
        lv_label_set_text(s_frac, t->frac ? t->frac : "—");
    } else {
        lv_label_set_text(s_tc, "—");
        lv_label_set_text(s_ts, "—");
        lv_label_set_text(s_p,  "—");
        lv_label_set_text(s_power_txt, "—");
        lv_bar_set_value(s_power_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(s_baro, "—");
        lv_label_set_text(s_flow, "—");
        lv_label_set_text(s_frac, "—");
    }

    /* LINK */
    lv_label_set_text(s_link, t->link ? "ONLINE" : "NO LINK");
    color_row(lv_obj_get_parent(s_link),
              t->link ? c_green : c_red);

    /* E-STOP — всегда показываем, даже без связи */
    lv_label_set_text(s_es, t->es ? "НАЖАТ" : "ОК");
    color_row(lv_obj_get_parent(s_es), t->es ? c_red : c_green);

    /* SAFETY */
    lv_label_set_text(s_safety,
                      !t->link ? "Н/Д" : (t->sa ? "OK" : "FAULT"));
    color_row(lv_obj_get_parent(s_safety),
              !t->link ? c_muted : (t->sa ? c_green : c_red));

    lv_label_set_text_fmt(s_level, "%lu", (unsigned long)t->lv);
    lv_label_set_text_fmt(s_stage, "%lu", (unsigned long)t->stg);
    lv_label_set_text(s_ver, t->version ? t->version : "v0.0");
    lv_label_set_text_fmt(s_crc, "%lu", (unsigned long)t->crc_err);

    /* Строка событий: заполняется из telemetry.ev[] в реальной прошивке */
    lv_label_set_text_fmt(s_ev[0], "t=%lus  run=%lu pause=%lu",
                          (unsigned long)t->elapsed, (unsigned long)t->run, (unsigned long)t->pause);
    lv_label_set_text_fmt(s_ev[1], "heads=%lu body=%lu tails=%lu",
                          (unsigned long)t->heads, (unsigned long)t->body, (unsigned long)t->tails);
    lv_label_set_text_fmt(s_ev[2], "db=%.1f r=%lu",
                          (float)t->db / 10.0f, (unsigned long)t->r);
}
