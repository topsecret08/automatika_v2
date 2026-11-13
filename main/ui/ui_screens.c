#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/state.h"
#include "core/events.h"
#include "control/ctrl_heater.h"
#include "control/safety_gate.h"
#include "control/ctrl_scale.h"
#include "control/ctrl_alcohol_balance.h"
#include "control/ctrl_time_predict.h"
#include "hal/hal_1wire.h"
#include "hal/hal_adc.h"
#include "ai/cache_manager.h"
#include "ai/page_predictor.h"
#include "ai/anomaly.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>

extern lv_obj_t* ui_make_page(const char *title);

// 1. Экран процесса
void ui_show_process(void) {
    lv_obj_t *c = ui_make_page("Процесс");
    process_state_t *s = state_get();
    time_prediction_t *tp = time_predict_get();

    static ui_card_t cards[4];
    cards[0] = (ui_card_t){"🌡","Куб",NULL,&s->cube_temp,"°C",0xFF3B30,NULL,NULL};
    cards[1] = (ui_card_t){"🌡","Царга",NULL,&s->column_temp,"°C",0x007AFF,NULL,NULL};
    cards[2] = (ui_card_t){"🔥","Мощность",NULL,&s->power_percent,"%",0xFF9500,NULL,NULL};
    cards[3] = (ui_card_t){"💧","Клапан",NULL,&s->valve_body_pct,"%",0x34C759,NULL,NULL};
    ui_build_card_grid(c, cards, 4);

    lv_obj_t *prog = lv_obj_create(c);
    lv_obj_set_size(prog, LV_PCT(100), 100);
    lv_obj_align(prog, LV_ALIGN_TOP_MID, 0, 220);
    ui_style_apply_card(prog);
    ui_progress_t p = { "Прогресс", &tp->progress_pct, 0x007AFF, NULL };
    ui_build_progress(prog, &p);

    lv_obj_t *info = lv_label_create(prog);
    char buf[128];
    snprintf(buf, sizeof(buf), "⏱ %s  Осталось %u:%02u",
             tp->eta_str, (unsigned)(tp->remaining_sec / 60),
             (unsigned)(tp->remaining_sec % 60));
    lv_label_set_text(info, buf);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 0, 40);

    static ui_button_t btns[3];
    extern void cb_resume(void*); extern void cb_pause(void*); extern void cb_stop(void*);
    btns[0] = (ui_button_t){"СТАРТ","▶",0x34C759,cb_resume,NULL};
    btns[1] = (ui_button_t){"ПАУЗА","⏸",0xFF9500,cb_pause,NULL};
    btns[2] = (ui_button_t){"СТОП","⏹",0xFF3B30,cb_stop,NULL};
    ui_build_button_bar(c, btns, 3);
}

// 2. Графики
void ui_show_graphs(void) {
    lv_obj_t *c = ui_make_page("Графики");
    static uint32_t colors[] = {0xFF3B30, 0x007AFF, 0x34C759, 0xFF9500};
    ui_chart_t cfg = {"Температуры", 2, colors, 60};
    lv_obj_t *ch = ui_build_chart(c, &cfg);
    lv_obj_align(ch, LV_ALIGN_TOP_MID, 0, 0);
    lv_chart_series_t *s1 = lv_chart_get_series_next(ch, NULL);
    lv_chart_series_t *s2 = lv_chart_get_series_next(ch, s1);
    for (int i = 0; i < 60; i++) {
        lv_chart_set_next_value(ch, s1, 60 + (i*7)%30);
        lv_chart_set_next_value(ch, s2, 75 + (i*3)%5);
    }
}

void ui_show_graphs_window(int minutes) { (void)minutes; ui_show_graphs(); }

// 3. Статистика
void ui_show_stats(void) {
    lv_obj_t *c = ui_make_page("Статистика");
    process_state_t *s = state_get();
    static char hbuf[16], bbuf[16], tbuf[16], pbuf[16];
    snprintf(hbuf, sizeof(hbuf), "%.0f мл", s->heads_collected_ml);
    snprintf(bbuf, sizeof(bbuf), "%.0f мл", s->body_collected_ml);
    snprintf(tbuf, sizeof(tbuf), "%.0f мл", s->tails_collected_ml);
    snprintf(pbuf, sizeof(pbuf), "%u", (unsigned)s->predec_count);

    static ui_card_t cards[] = {
        {"🍶","Головы",hbuf,NULL,NULL,0xFF3B30,NULL,NULL},
        {"🥃","Тело",bbuf,NULL,NULL,0x34C759,NULL,NULL},
        {"🌊","Хвосты",tbuf,NULL,NULL,0xFF9500,NULL,NULL},
        {"⚡","Предекр.",pbuf,NULL,NULL,0xAF52DE,NULL,NULL},
    };
    ui_build_card_grid(c, cards, 4);
}

void ui_stats_extended(void) {
    lv_obj_t *c = ui_make_page("Расширенная статистика");
    process_state_t *s = state_get();
    int64_t uptime = esp_timer_get_time() / 1000000;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "Процесс:\n  Время: %u ч %u мин\n  Отобрано: %.0f мл\n"
        "  Клапан: %.0f%%\n  Предекр: %u\n\n"
        "Система:\n  Uptime: %lld д %lld ч\n"
        "  Heap: %u КБ\n  PSRAM: %u КБ\n  SRAM: %u КБ",
        (unsigned)(s->elapsed_sec/3600), (unsigned)((s->elapsed_sec%3600)/60),
        s->body_collected_ml, s->valve_body_pct, (unsigned)s->predec_count,
        (long long)(uptime/86400), (long long)((uptime%86400)/3600),
        (unsigned)(esp_get_free_heap_size()/1024),
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024),
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)/1024));
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
}

// 4. Логи
void ui_show_logs(void) {
    lv_obj_t *c = ui_make_page("Журнал событий");
    size_t n;
    event_t *ev = events_get(&n);
    lv_obj_t *list = lv_obj_create(c);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_row(list, 6, 0);

    for (size_t i = 0; i < n && i < 30; i++) {
        size_t idx = n - 1 - i;
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, LV_PCT(100), 44);
        ui_style_apply_card(row);
        uint32_t col = 0x007AFF;
        if (ev[idx].type == 1) col = 0xFFCC00;
        if (ev[idx].type == 2) col = 0xFF3B30;
        if (ev[idx].type == 3) col = 0xFF3B30;
        if (ev[idx].type == 5) col = 0x34C759;

        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_bg_color(dot, lv_color_hex(col), 0);
        lv_obj_set_style_radius(dot, 4, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *msg = lv_label_create(row);
        lv_label_set_text(msg, ev[idx].msg);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);
        lv_obj_align(msg, LV_ALIGN_LEFT_MID, 20, 0);
    }
}

// 5. Ручное управление
static float s_manual_power = 0;
static void on_manual_power(float v) { safety_gate_apply("ui_power", v); }
void ui_show_manual(void) {
    lv_obj_t *c = ui_make_page("Ручное управление");
    static ui_slider_t sl = { "Мощность", 0, 100, 5, &s_manual_power, "%", on_manual_power };
    ui_build_slider(c, &sl);

    static ui_button_t btns[] = {
        {"Насос","💧",0x007AFF,NULL,NULL},
        {"Мешалка","🌀",0xAF52DE,NULL,NULL},
        {"АВАРИЯ","🛑",0xFF3B30,NULL,NULL},
    };
    extern void cb_toggle_pump(void*); extern void cb_toggle_stirrer(void*);
    extern void cb_emergency_stop(void*);
    btns[0].on_click = cb_toggle_pump;
    btns[1].on_click = cb_toggle_stirrer;
    btns[2].on_click = cb_emergency_stop;
    ui_build_button_bar(c, btns, 3);
}

// 6. Прогноз
void ui_show_forecast(void) {
    lv_obj_t *c = ui_make_page("Прогнозы");
    time_prediction_t *tp = time_predict_get();
    char buf[256];
    snprintf(buf, sizeof(buf),
        "ETA: %s\nОсталось: %u:%02u\n"
        "Прогресс: %.0f%%\nСкорость: %.2f°C/мин",
        tp->eta_str, (unsigned)(tp->remaining_sec/60),
        (unsigned)(tp->remaining_sec%60),
        tp->progress_pct, tp->rate_per_min);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
}

// 7. Тепловая карта
void ui_show_heatmap(void) {
    lv_obj_t *c = ui_make_page("Тепловая карта");
    const char *labels[] = {"Царга верх","Царга низ","Куб","Дефлегматор","Холодильник"};
    float temps[] = {77.0f, 78.5f, 82.3f, 45.0f, 30.0f};
    uint32_t colors[] = {0xFF3B30, 0xFF9500, 0xFFCC00, 0x007AFF, 0x34C759};

    for (int i = 0; i < 5; i++) {
        lv_obj_t *row = lv_obj_create(c);
        lv_obj_set_size(row, LV_PCT(100), 44);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text_fmt(lbl, "%s: %.1f°C", labels[i], temps[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *bar = lv_bar_create(row);
        lv_obj_set_size(bar, 150, 20);
        lv_bar_set_range(bar, 0, 120);
        lv_bar_set_value(bar, (int)temps[i], LV_ANIM_ON);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), LV_PART_INDICATOR);
        lv_obj_align(bar, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

// 8. Схема
void ui_show_scheme(void) {
    lv_obj_t *c = ui_make_page("Схема установки");
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, "Схема установки\n\n(см. /sdcard/schemes/)");
    lv_obj_center(l);
}

void ui_show_gabriel(void) { ui_calc_gabriel(); }