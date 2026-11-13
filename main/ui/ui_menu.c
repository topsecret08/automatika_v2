#include "ui_menu.h"
#include "ui.h"
#include "ui_glass_theme.h"
#include "ui_screen_home.h"
#include "ui_screen_advanced.h"
#include "ui_screen_selection.h"
#include "haptic.h"

#include "config.h"
#include "core/health.h"
#include "core/process_control.h"
#include "core/profiles.h"
#include "core/state.h"
#include "control/ctrl_dose.h"
#include "control/fraction_logic.h"
#include "web/api_auth.h"

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern lv_obj_t *ui_make_page(const char *title);

// Текущее положение меню: нужно и тачу, и энкодеру.
static int s_tab_index = 0;
static int s_item_index = 0;
static int s_level = 0;   // 0 — корень, 1 — группа

static const char *fraction_ru(fraction_t f) {
    switch (f) {
    case FRAC_HEADS: return "головы";
    case FRAC_BODY:  return "тело";
    case FRAC_TAILS: return "хвосты";
    case FRAC_DONE:  return "готово";
    default:         return "нет данных";
    }
}

// ------------------------------------------------------------------
// Действия
// ------------------------------------------------------------------
static void enter_dashboard(void) { ui_screen_home_create(); }

static void enter_fraction(void);
static void enter_profiles(void);
static void enter_calibration(void);
static void enter_fsel(void)     { ui_screen_selection_dashboard_create(); }
static void enter_autotune(void) { ui_settings_pid(); }
static void enter_sensors(void)  { ui_diag_ds18b20(); }
static void enter_log(void)      { ui_show_logs(); }
static void enter_actuators(void){ ui_diag_test_outputs(); }
static void enter_health(void)   { ui_stats_extended(); }
static void enter_network(void);
static void enter_ota(void)      { ui_fota_menu(); }
static void enter_api(void);
static void enter_lang(void)     { ui_settings_general(); }
static void enter_about(void);
static void enter_advanced(void)  { ui_screen_advanced_create(); }

static void enter_start(void) {
    if (process_control_start(NULL) == ESP_OK) {
        ui_toast("Процесс запущен");
        ui_screen_home_create();
    } else {
        ui_toast("Запуск запрещён: E-STOP или защита");
    }
}

static void enter_stop(void) {
    process_control_stop();
    ui_toast("Процесс остановлен");
    ui_screen_home_create();
}

// ------------------------------------------------------------------
// Дерево
// ------------------------------------------------------------------
ui_menu_item menu_work[] = {
    { "Дашборд",         enter_dashboard, NULL, 0 },
    { "Текущая фракция", enter_fraction,  NULL, 0 },
    { "Отбор",           enter_fsel,      NULL, 0 },
    { "СТАРТ",           enter_start,     NULL, 0 },
    { "СТОП",            enter_stop,      NULL, 0 },
};

ui_menu_item menu_prepare[] = {
    { "Профили",           enter_profiles,    NULL, 0 },
    { "Калибровка насоса", enter_calibration, NULL, 0 },
    { "PID-автотюн",       enter_autotune,    NULL, 0 },
    { "Проверка датчиков", enter_sensors,     NULL, 0 },
};

ui_menu_item menu_diagnose[] = {
    { "Журнал событий",    enter_log,       NULL, 0 },
    { "Проверка приводов", enter_actuators, NULL, 0 },
    { "Состояние",         enter_health,    NULL, 0 },
    { "Сеть",              enter_network,   NULL, 0 },
};

ui_menu_item menu_system[] = {
    { "Обновление",   enter_ota,   NULL, 0 },
    { "API-ключ",     enter_api,   NULL, 0 },
    { "Язык",         enter_lang,  NULL, 0 },
    { "Отбор/регулятор", enter_advanced, NULL, 0 },
    { "О устройстве", enter_about, NULL, 0 },
};

typedef struct {
    const char   *label;
    const char   *symbol;
    ui_menu_item *items;
    int           count;
} ui_tab_t;

static ui_tab_t s_tabs[] = {
    { "Работа",       LV_SYMBOL_PLAY,     menu_work,
      (int)(sizeof(menu_work) / sizeof(menu_work[0])) },
    { "Подготовка",   LV_SYMBOL_LIST,     menu_prepare,
      (int)(sizeof(menu_prepare) / sizeof(menu_prepare[0])) },
    { "Диагностика",  LV_SYMBOL_WARNING,  menu_diagnose,
      (int)(sizeof(menu_diagnose) / sizeof(menu_diagnose[0])) },
    { "Система",      LV_SYMBOL_SETTINGS, menu_system,
      (int)(sizeof(menu_system) / sizeof(menu_system[0])) },
};

int ui_menu_tab_count(void) {
    return (int)(sizeof(s_tabs) / sizeof(s_tabs[0]));
}

void ui_menu_set_state(int tab, int level, int item) {
    if (tab >= 0 && tab < ui_menu_tab_count()) s_tab_index = tab;
    s_level = (level == 0) ? 0 : 1;
    s_item_index = item;
}

const char *ui_menu_tab_label(int tab) {
    if (tab < 0 || tab >= ui_menu_tab_count()) return "";
    return s_tabs[tab].label;
}

// ------------------------------------------------------------------
// Экран-список (touch)
// ------------------------------------------------------------------
static void tab_click_cb(lv_event_t *event) {
    int tab = (int)(intptr_t)lv_event_get_user_data(event);
    s_tab_index = tab;
    s_item_index = 0;
    s_level = 1;
    ui_menu_show_group(tab, 0);
}

static void item_click_cb(lv_event_t *event) {
    ui_menu_item *item = (ui_menu_item *)lv_event_get_user_data(event);
    if (item && item->on_enter) item->on_enter();
}

static void back_click_cb(lv_event_t *event) {
    int tab = (int)(intptr_t)lv_event_get_user_data(event);
    s_tab_index = tab;
    s_item_index = 0;
    s_level = 0;
    ui_menu_show_root(tab);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

void ui_menu_build_root(lv_obj_t *parent, int selected_tab) {
    if (!parent) return;

    lv_obj_set_style_bg_color(parent, UI_GLASS_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    make_label(parent, "Меню", &lv_font_cyr_24, UI_GLASS_TEXT);

    for (int i = 0; i < ui_menu_tab_count(); i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, 54);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_set_style_bg_color(btn,
            (i == selected_tab) ? UI_GLASS_ACCENT : UI_GLASS_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_left(btn, 16, 0);
        lv_obj_set_style_pad_column(btn, 12, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        make_label(btn, s_tabs[i].symbol, &lv_font_cyr_24, lv_color_white());
        make_label(btn, s_tabs[i].label, &lv_font_cyr_16, lv_color_white());

        lv_obj_add_event_cb(btn, tab_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }
}

void ui_menu_build_group(lv_obj_t *parent, int tab, int selected_item) {
    if (!parent) return;
    if (tab < 0 || tab >= ui_menu_tab_count()) return;

    ui_tab_t *t = &s_tabs[tab];

    lv_obj_set_style_bg_color(parent, UI_GLASS_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Шапка: «Назад» + название группы.
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
    lv_obj_set_style_bg_color(back, UI_GLASS_CARD, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_t *back_label = make_label(back, LV_SYMBOL_LEFT, &lv_font_cyr_16, lv_color_white());
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, back_click_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)tab);

    make_label(header, t->label, &lv_font_cyr_24, UI_GLASS_TEXT);

    for (int i = 0; i < t->count; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, 42);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn,
            (i == selected_item) ? UI_GLASS_ACCENT : UI_GLASS_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_left(btn, 14, 0);
        lv_obj_set_style_pad_right(btn, 14, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        make_label(btn, t->items[i].label, &lv_font_cyr_16, lv_color_white());
        make_label(btn, LV_SYMBOL_RIGHT, &lv_font_cyr_16, UI_GLASS_TEXT_2);

        lv_obj_add_event_cb(btn, item_click_cb, LV_EVENT_CLICKED,
                            (void *)&t->items[i]);
    }
}

// ------------------------------------------------------------------
// Энкодер + flick-жесты
// ------------------------------------------------------------------
static int64_t s_last_rotate_us = 0;
static int s_acc = 0;

void ui_menu_next_tab(void) {
    s_level = 0;
    s_tab_index = (s_tab_index + 1) % ui_menu_tab_count();
    ui_menu_show_root(s_tab_index);
}

void ui_menu_prev_tab(void) {
    s_level = 0;
    s_tab_index = (s_tab_index + ui_menu_tab_count() - 1) % ui_menu_tab_count();
    ui_menu_show_root(s_tab_index);
}

void ui_menu_enter(void) { ui_menu_encoder_press(); }

void ui_menu_back(void) {
    if (s_level == 1) {
        s_level = 0;
        ui_menu_show_root(s_tab_index);
    }
}

int ui_menu_current_tab(void) { return s_tab_index; }
int ui_menu_current_level(void) { return s_level; }

void ui_menu_encoder_rotate(int dir) {
    if (dir == 0) return;

    if (ui_screen_selection_modal_active() && ui_screen_selection_modal_rotate(dir)) {
        haptic_short();
        return;
    }

    int64_t now = esp_timer_get_time();
    int64_t dt = now - s_last_rotate_us;

    // Медленное вращение не накапливается бесконечно: пауза >150 мс
    // сбрасывает счётчик, иначе любой следующий шаг выглядел бы flick-ом.
    if (dt > 150000) s_acc = 0;

    // Быстрый «пролёт» энкодера (3 шага < 150 мс) — свайп между табами.
    if (s_last_rotate_us != 0 && dt < 150000 && abs(s_acc) >= 3) {
        s_acc = 0;
        s_last_rotate_us = now;
        if (dir > 0) ui_menu_next_tab();
        else ui_menu_prev_tab();
        haptic_short();
        return;
    }

    s_last_rotate_us = now;
    s_acc += dir;

    if (s_level == 0) {
        s_tab_index = (s_tab_index + dir + ui_menu_tab_count()) % ui_menu_tab_count();
        ui_menu_show_root(s_tab_index);
    } else {
        int count = s_tabs[s_tab_index].count;
        if (count <= 0) return;
        s_item_index = (s_item_index + dir + count) % count;
        ui_menu_show_group(s_tab_index, s_item_index);
    }
}

void ui_menu_encoder_press(void) {
    if (ui_screen_selection_modal_active() && ui_screen_selection_modal_press()) {
        haptic_short();
        return;
    }

    if (s_level == 0) {
        s_level = 1;
        s_item_index = 0;
        ui_menu_show_group(s_tab_index, s_item_index);
        haptic_short();
        return;
    }

    ui_menu_item *item = &s_tabs[s_tab_index].items[s_item_index];
    if (item->on_enter) item->on_enter();
    haptic_short();
}

void ui_menu_encoder_long_press(void) {
    if (ui_screen_selection_modal_active()) return;
    if (s_level == 1) {
        s_level = 0;
        ui_menu_show_root(s_tab_index);
        haptic_short();
    }
}

// ------------------------------------------------------------------
// Информационные экраны
// ------------------------------------------------------------------
static lv_obj_t *menu_label(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, &lv_font_cyr_16, 0);
    lv_obj_set_style_text_color(label, UI_GLASS_TEXT, 0);
    return label;
}

static void enter_fraction(void) {
    lv_obj_t *c = ui_make_page("Текущая фракция");
    process_state_t *st = state_get();

    char buf[256];
    snprintf(buf, sizeof(buf),
             "Сейчас: %s\nЭтап: %s\n\n"
             "Пороги:\n"
             "головы → тело: %.1f °C / %.0f мл\n"
             "тело → хвосты: %.1f °C / %.0f мл\n"
             "хвосты → готово: %.1f °C",
             fraction_ru(fraction_get()),
             state_stage_name(st->stage),
             (double)FRAC_HEADS_END_T, (double)FRAC_HEADS_MAX_ML,
             (double)FRAC_BODY_END_T, (double)FRAC_BODY_MAX_ML,
             (double)FRAC_TAILS_END_T);
    menu_label(c, buf);
    menu_label(c, "Переключение выполняет автоматика. Ручной выбор — в веб-интерфейсе.");
}

static void enter_profiles(void) {
    lv_obj_t *c = ui_make_page("Профили");

    static char names[PROFILES_MAX_COUNT][PROFILES_MAX_NAME];
    int count = profiles_list(names, PROFILES_MAX_COUNT);

    char buf[640];
    int off = snprintf(buf, sizeof(buf), "Сохранено профилей: %d\n\n", count);
    for (int i = 0; i < count && off < (int)sizeof(buf) - 1; i++) {
        off += snprintf(buf + off, sizeof(buf) - (size_t)off, "• %s\n", names[i]);
    }
    if (count == 0) {
        snprintf(buf, sizeof(buf), "Профилей нет.\nСоздайте профиль в веб-интерфейсе.");
    }
    menu_label(c, buf);
    menu_label(c, "Выбор и создание профиля — в PWA, раздел «Подготовка».");
}

static void dose_10_cb(lv_event_t *event) {
    (void)event;
    bool ok = ctrl_dose_volume(10.0f, 60.0f);
    ui_toast(ok ? "Дозирование 10 мл запущено" : "Дозатор занят или недоступен");
}

static void enter_calibration(void) {
    lv_obj_t *c = ui_make_page("Калибровка насоса");
    menu_label(c,
        "1. Поставьте трубку в мерный цилиндр.\n"
        "2. Нажмите «10 мл» и замерьте фактический объём.\n"
        "3. Пересчитайте шаги: steps = old × факт / 10.\n"
        "4. Сохраните steps_per_ml в веб-интерфейсе.");

    lv_obj_t *btn = lv_btn_create(c);
    lv_obj_set_size(btn, 180, 44);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, UI_GLASS_ACCENT, 0);
    lv_obj_t *lbl = make_label(btn, "10 мл", &lv_font_cyr_16, lv_color_white());
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, dose_10_cb, LV_EVENT_CLICKED, NULL);
}

static void enter_network(void) {
    lv_obj_t *c = ui_make_page("Сеть");

    int rssi = 0, sd = 0, db = 0, sensors = 0;
    unsigned heap = 0;
    int uptime = 0;
    cJSON *root = cJSON_Parse(health_json());
    if (root) {
        cJSON *v;
        if ((v = cJSON_GetObjectItem(root, "wifi_rssi"))) rssi = v->valueint;
        if ((v = cJSON_GetObjectItem(root, "sd"))) sd = v->valueint;
        if ((v = cJSON_GetObjectItem(root, "db"))) db = v->valueint;
        if ((v = cJSON_GetObjectItem(root, "sensors"))) sensors = v->valueint;
        if ((v = cJSON_GetObjectItem(root, "free_heap"))) heap = (unsigned)v->valuedouble;
        if ((v = cJSON_GetObjectItem(root, "uptime_s"))) uptime = (int)v->valuedouble;
        cJSON_Delete(root);
    }

    char buf[320];
    snprintf(buf, sizeof(buf),
             "Wi-Fi RSSI: %d dBm\n"
             "SD-карта: %s\n"
             "SQLite: %s\n"
             "Датчики 1-Wire: %d\n"
             "Свободно RAM: %u КБ\n"
             "Uptime: %d мин",
             rssi, sd ? "есть" : "нет", db ? "ok" : "нет",
             sensors, heap / 1024, uptime / 60);
    menu_label(c, buf);
    menu_label(c, "Настройка Wi-Fi, MQTT и Telegram — в веб-интерфейсе.");
}

static void qr_cb(lv_event_t *event) {
    (void)event;
    api_auth_show_qr();
}

static void enter_api(void) {
    lv_obj_t *c = ui_make_page("API-ключ");
    menu_label(c,
        "Ключ задаётся в PWA: Настройки → API.\n"
        "Пока ключ не создан, доступна одноразовая генерация.\n"
        "QR-код содержит API-ключ: показывайте только доверенным лицам.");

    lv_obj_t *btn = lv_btn_create(c);
    lv_obj_set_size(btn, 180, 44);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, UI_GLASS_ACCENT, 0);
    lv_obj_t *lbl = make_label(btn, "Показать QR", &lv_font_cyr_16, lv_color_white());
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, qr_cb, LV_EVENT_CLICKED, NULL);
}

static void enter_about(void) {
    lv_obj_t *c = ui_make_page("О устройстве");

    int64_t uptime = esp_timer_get_time() / 1000000;
    char buf[320];
    snprintf(buf, sizeof(buf),
             "%s\nВерсия: %s\n\n"
             "Uptime: %lld ч %lld мин\n"
             "Heap: %u КБ\n"
             "PSRAM: %u КБ",
             FW_NAME, FW_VERSION,
             (long long)(uptime / 3600), (long long)((uptime % 3600) / 60),
             (unsigned)(esp_get_free_heap_size() / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    menu_label(c, buf);
    menu_label(c, "Automatika · ESP32-S3 · ESP-IDF");
}
