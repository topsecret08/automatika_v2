#include "ui_screen_home.h"
#include "ui.h"
#include "ui_glass_theme.h"
#include "ui_menu.h"

#include "core/estop.h"
#include "core/process_control.h"
#include "core/session_manager.h"
#include "core/state.h"
#include "control/ctrl_safety.h"
#include "control/fraction_logic.h"
#include "hal/baro.h"
#include "hal/hal_1wire.h"

#include <math.h>
#include <stdio.h>

static lv_obj_t *s_steam;
static lv_obj_t *s_cube;
static lv_obj_t *s_power;
static lv_obj_t *s_badge;
static lv_obj_t *s_stage;
static lv_obj_t *s_footer;
static lv_obj_t *s_dot;
static lv_obj_t *s_start_btn;
static lv_timer_t *s_timer;

static void screen_delete_cb(lv_event_t *event) {
    (void)event;
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    s_steam = s_cube = s_power = s_badge = s_stage = s_footer = s_dot = NULL;
    s_start_btn = NULL;
}

static void update_dashboard(lv_timer_t *timer) {
    (void)timer;
    if (!s_steam) return;

    process_state_t *st = state_get();

    float cube = hal_1wire_read(0);
    float steam = hal_1wire_read(1);
    if (!isfinite(steam) || steam < -50.0f) steam = st->column_temp;
    steam = baro_correct_boiling_point(steam, baro_get_pressure_hpa());
    if (!isfinite(cube) || cube < -50.0f) cube = st->cube_temp;

    char buf[48];

    if (isfinite(steam) && steam > -50.0f)
        snprintf(buf, sizeof(buf), "%.1f°", steam);
    else
        snprintf(buf, sizeof(buf), "--.-°");
    lv_label_set_text(s_steam, buf);

    if (isfinite(cube) && cube > -50.0f)
        snprintf(buf, sizeof(buf), "%.1f°", cube);
    else
        snprintf(buf, sizeof(buf), "--.-°");
    lv_label_set_text(s_cube, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", st->power_percent);
    lv_label_set_text(s_power, buf);

    fraction_t f = fraction_get();
    const char *name = "—";
    lv_color_t color = UI_GLASS_CARD_2;
    lv_color_t text = UI_GLASS_TEXT;
    switch (f) {
    case FRAC_HEADS: name = "ГОЛОВЫ"; color = UI_GLASS_YELLOW; text = lv_color_black(); break;
    case FRAC_BODY:  name = "ТЕЛО";   color = UI_GLASS_ACCENT; text = lv_color_white(); break;
    case FRAC_TAILS: name = "ХВОСТЫ"; color = UI_GLASS_ORANGE; text = lv_color_black(); break;
    case FRAC_DONE:  name = "ГОТОВО"; color = UI_GLASS_GREEN; text = lv_color_white(); break;
    default: break;
    }
    lv_label_set_text(s_badge, name);
    lv_obj_set_style_bg_color(s_badge, color, 0);
    lv_obj_set_style_text_color(s_badge, text, 0);

    lv_label_set_text(s_stage, state_stage_name(st->stage));

    float volume = st->heads_collected_ml + st->body_collected_ml +
                   st->tails_collected_ml;
    snprintf(buf, sizeof(buf), "Отбор %.0f мл/мин · %.0f мл",
             st->body_rate_ml_min, volume);
    lv_label_set_text(s_footer, buf);

    bool alarm = estop_is_pressed() || !ctrl_safety_ok();
    lv_color_t dot = alarm ? UI_GLASS_RED
        : (st->is_running ? UI_GLASS_GREEN : UI_GLASS_TEXT_2);
    lv_obj_set_style_bg_color(s_dot, dot, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);

    if (s_start_btn) {
        if (st->is_running || alarm)
            lv_obj_add_state(s_start_btn, LV_STATE_DISABLED);
        else
            lv_obj_clear_state(s_start_btn, LV_STATE_DISABLED);
    }
}

static void start_cb(lv_event_t *event) {
    (void)event;
    esp_err_t err = process_control_start(NULL);
    if (err == ESP_OK) ui_toast("Процесс запущен");
    else ui_toast("Запуск запрещён: E-STOP или защита");
}

static void stop_cb(lv_event_t *event) {
    (void)event;
    process_control_stop();
    ui_toast("Процесс остановлен");
}

static void menu_cb(lv_event_t *event) {
    (void)event;
    session_minimize();
    ui_menu_show_root(0);
}

static lv_obj_t *make_caption(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, UI_GLASS_TEXT_2, 0);
    lv_obj_set_style_text_font(label, &lv_font_cyr_16, 0);
    return label;
}

static lv_obj_t *make_kpi(lv_obj_t *parent, const char *caption) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(48));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, UI_GLASS_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 2, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    make_caption(card, caption);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--.-°");
    lv_obj_set_style_text_color(value, UI_GLASS_TEXT, 0);
    lv_obj_set_style_text_font(value, &lv_font_cyr_24, 0);
    return value;
}

void ui_screen_home_build(lv_obj_t *parent) {
    if (!parent) return;

    lv_obj_clean(parent);
    lv_obj_set_style_bg_color(parent, UI_GLASS_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Шапка: заголовок, этап, индикатор состояния.
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, 32);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Дистилляция");
    lv_obj_set_style_text_color(title, UI_GLASS_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_cyr_24, 0);

    lv_obj_t *right = lv_obj_create(header);
    lv_obj_set_size(right, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 8, 0);

    s_stage = lv_label_create(right);
    lv_label_set_text(s_stage, "Ожидание");
    lv_obj_set_style_text_color(s_stage, UI_GLASS_TEXT_2, 0);
    lv_obj_set_style_text_font(s_stage, &lv_font_cyr_16, 0);

    s_dot = lv_obj_create(right);
    lv_obj_set_size(s_dot, 10, 10);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, UI_GLASS_TEXT_2, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);
    lv_obj_clear_flag(s_dot, LV_OBJ_FLAG_SCROLLABLE);

    // Главная карточка: скорректированная температура пара.
    lv_obj_t *main = ui_glass_card_create(parent);
    lv_obj_set_style_pad_all(main, 12, 0);
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    make_caption(main, "ПАР (СКОРР.)");

    lv_obj_t *value_row = lv_obj_create(main);
    lv_obj_set_width(value_row, lv_pct(100));
    lv_obj_set_height(value_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(value_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(value_row, 0, 0);
    lv_obj_set_style_pad_all(value_row, 0, 0);
    lv_obj_clear_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_steam = lv_label_create(value_row);
    lv_label_set_text(s_steam, "--.-°");
    lv_obj_set_style_text_color(s_steam, UI_GLASS_TEXT, 0);
    lv_obj_set_style_text_font(s_steam, &lv_font_cyr_24, 0);

    s_badge = lv_label_create(value_row);
    lv_label_set_text(s_badge, "—");
    lv_obj_set_style_bg_color(s_badge, UI_GLASS_CARD_2, 0);
    lv_obj_set_style_bg_opa(s_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_badge, 8, 0);
    lv_obj_set_style_pad_hor(s_badge, 10, 0);
    lv_obj_set_style_pad_top(s_badge, 4, 0);
    lv_obj_set_style_pad_bottom(s_badge, 4, 0);
    lv_obj_set_style_text_color(s_badge, UI_GLASS_TEXT, 0);
    lv_obj_set_style_text_font(s_badge, &lv_font_cyr_16, 0);

    s_footer = lv_label_create(main);
    lv_label_set_text(s_footer, "Отбор 0 мл/мин · 0 мл");
    lv_obj_set_style_text_color(s_footer, UI_GLASS_TEXT_2, 0);
    lv_obj_set_style_text_font(s_footer, &lv_font_cyr_16, 0);

    // Два KPI: куб и мощность.
    lv_obj_t *kpi_row = lv_obj_create(parent);
    lv_obj_set_width(kpi_row, lv_pct(100));
    lv_obj_set_height(kpi_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(kpi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kpi_row, 0, 0);
    lv_obj_set_style_pad_all(kpi_row, 0, 0);
    lv_obj_clear_flag(kpi_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(kpi_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(kpi_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_cube = make_kpi(kpi_row, "КУБ");
    s_power = make_kpi(kpi_row, "МОЩНОСТЬ");

    // Кнопки. flex_grow делит ширину поровну.
    lv_obj_t *buttons = lv_obj_create(parent);
    lv_obj_set_width(buttons, lv_pct(100));
    lv_obj_set_height(buttons, 44);
    lv_obj_set_style_bg_opa(buttons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(buttons, 0, 0);
    lv_obj_set_style_pad_all(buttons, 0, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 8, 0);

    lv_obj_t *start = ui_glass_btn_create(buttons, "Старт", UI_GLASS_ACCENT);
    s_start_btn = start;
    lv_obj_set_flex_grow(start, 1);
    lv_obj_add_event_cb(start, start_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *stop = ui_glass_btn_create(buttons, "Стоп", UI_GLASS_RED);
    lv_obj_set_flex_grow(stop, 1);
    lv_obj_add_event_cb(stop, stop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *menu = ui_glass_btn_create(buttons, "Свернуть", UI_GLASS_CARD_2);
    lv_obj_set_flex_grow(menu, 1);
    lv_obj_add_event_cb(menu, menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(parent, screen_delete_cb, LV_EVENT_DELETE, NULL);
    s_timer = lv_timer_create(update_dashboard, 500, NULL);
    update_dashboard(NULL);
}
