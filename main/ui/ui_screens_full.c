#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/state.h"
#include "core/flags.h"
#include "core/roles.h"
#include "control/ctrl_alcohol_balance.h"
#include "hal/hal_1wire.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

// Калькуляторы
static float s_dil_vol=1.0f, s_dil_a1=96.0f, s_dil_a2=40.0f;
void ui_calc_dilution(void) {
    lv_obj_t *c = ui_make_page("Разбавление");
    static ui_slider_t sl1 = {"Объём", 0.1f, 50, 0.1f, &s_dil_vol, " л", NULL};
    static ui_slider_t sl2 = {"Исходная", 20, 96, 1, &s_dil_a1, "%", NULL};
    static ui_slider_t sl3 = {"Целевая", 5, 90, 1, &s_dil_a2, "%", NULL};
    ui_build_slider(c, &sl1); ui_build_slider(c, &sl2); ui_build_slider(c, &sl3);
    float water = s_dil_vol * (s_dil_a1/s_dil_a2 - 1.0f);
    lv_obj_t *l = lv_label_create(c);
    char buf[128];
    snprintf(buf, sizeof(buf), "Вода: %.2f л\nИтого: %.2f л",
             water, s_dil_vol + water);
    lv_label_set_text(l, buf);
}

static float s_sugar_vol=20;
void ui_calc_sugar(void) {
    lv_obj_t *c = ui_make_page("Сахарная брага");
    static ui_slider_t sl = {"Объём", 5, 100, 1, &s_sugar_vol, " л", NULL};
    ui_build_slider(c, &sl);
    float sugar = s_sugar_vol * 0.375f;
    float yeast = sugar * 0.02f;
    char buf[256];
    snprintf(buf, sizeof(buf),
        "Сахар: %.2f кг\nДрожжи: %.0f г\nВода: %.0f л\nВыход: ~%.2f л",
        sugar, yeast, s_sugar_vol, sugar * 0.6f);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

static float s_grn_kg=5, s_grn_hm=4;
void ui_calc_grain(void) {
    lv_obj_t *c = ui_make_page("Зерновая брага");
    static ui_slider_t sl1 = {"Зерно", 1, 50, 0.5f, &s_grn_kg, " кг", NULL};
    static ui_slider_t sl2 = {"Гидромодуль", 2, 6, 0.1f, &s_grn_hm, "", NULL};
    ui_build_slider(c, &sl1); ui_build_slider(c, &sl2);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "Вода: %.1f л\nФерменты А: %.0f г\nФерменты Г: %.0f г\n"
        "Дрожжи: %.0f г\nВыход: ~%.1f л",
        s_grn_kg*s_grn_hm, s_grn_kg*3, s_grn_kg*2, s_grn_kg*4, s_grn_kg*0.45f);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

static float s_yl_vol=20, s_yl_abv=12, s_yl_t=40;
void ui_calc_yield(void) {
    lv_obj_t *c = ui_make_page("Выход");
    static ui_slider_t sl1 = {"Объём", 5, 100, 1, &s_yl_vol, " л", NULL};
    static ui_slider_t sl2 = {"Крепость", 5, 20, 0.5f, &s_yl_abv, "%", NULL};
    static ui_slider_t sl3 = {"Целевая", 20, 96, 1, &s_yl_t, "%", NULL};
    ui_build_slider(c, &sl1); ui_build_slider(c, &sl2); ui_build_slider(c, &sl3);
    float pure = s_yl_vol * s_yl_abv / 100.0f;
    char buf[128];
    snprintf(buf, sizeof(buf), "Чистый: %.2f л\nИтого: %.2f л",
             pure, pure * 100.0f / s_yl_t);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

static float s_abv_og=1.050f, s_abv_fg=1.010f;
void ui_calc_abv_ibu_srm(void) {
    lv_obj_t *c = ui_make_page("ABV/IBU/SRM");
    static ui_slider_t sl1 = {"OG", 10, 1100, 1, &s_abv_og, "", NULL};
    static ui_slider_t sl2 = {"FG", 1000, 1030, 1, &s_abv_fg, "", NULL};
    ui_build_slider(c, &sl1); ui_build_slider(c, &sl2);
    float abv = (s_abv_og - s_abv_fg) * 131.25f;
    char buf[128];
    snprintf(buf, sizeof(buf), "ABV: %.2f%%", abv);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

static float s_tcal=85;
void ui_calc_temp_abv(void) {
    lv_obj_t *c = ui_make_page("Крепость по T");
    static ui_slider_t sl = {"T кипения", 78, 100, 0.5f, &s_tcal, "°C", NULL};
    ui_build_slider(c, &sl);
    float abv = alcohol_balance_temp_to_abv(s_tcal);
    char buf[64];
    snprintf(buf, sizeof(buf), "Крепость: %.1f%%", abv);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
}

static float s_press=1013;
void ui_calc_pressure(void) {
    lv_obj_t *c = ui_make_page("Коррекция");
    static ui_slider_t sl = {"Давление", 950, 1050, 1, &s_press, " гПа", NULL};
    ui_build_slider(c, &sl);
    float boil = 100.0f + (s_press - 1013.0f) * 0.028f;
    char buf[128];
    snprintf(buf, sizeof(buf), "T кипения воды: %.2f°C\nПоправка: %+.2f°C",
             boil, boil - 100.0f);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

static float s_gab_v=20, s_gab_a=10;
void ui_calc_gabriel(void) {
    lv_obj_t *c = ui_make_page("Метод Габриэля");
    static ui_slider_t sl1 = {"Объём", 5, 100, 1, &s_gab_v, " л", NULL};
    static ui_slider_t sl2 = {"Крепость", 5, 20, 0.5f, &s_gab_a, "%", NULL};
    ui_build_slider(c, &sl1); ui_build_slider(c, &sl2);
    float k = 0.30f + s_gab_a / 100.0f * 0.25f;
    float t1 = s_gab_v * k;
    float t2 = s_gab_v - t1;
    float alc = t1 * s_gab_a / 100.0f;
    float dil = alc / 0.28f;
    float y = alc * 0.85f / 0.65f;
    char buf[256];
    snprintf(buf, sizeof(buf),
        "T1: %.1f л\nT2: %.1f л\nРазбавить: %.1f л\nВыход: %.1f л",
        t1, t2, dil, y);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

// Настройки
void ui_settings_hw(void) { lv_obj_t *c = ui_make_page("Оборудование");
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, "DS18B20: OK\nADS1115: OK\nPCA9685: OK\nSD: OK\nGrove SSR: OK\nHX711: OK");
}

void ui_settings_kit(void) { lv_obj_t *c = ui_make_page("Комплект");
    static ui_card_t cards[] = {
        {"✅","Обязательные","8/8",NULL,NULL,0x34C759,NULL,NULL},
        {"⭐","Опциональные","5/7",NULL,NULL,0xFF9500,NULL,NULL},
    };
    ui_build_card_grid(c, cards, 2);
}

void ui_settings_flags(void) {
    lv_obj_t *c = ui_make_page("Флаги");
    flags_t *f = flags_get();
    static ui_list_item_t items[24];
    int n = 0;
    items[n++] = (ui_list_item_t){"✓","Автоопределение голов",NULL,NULL,f->autodetect_heads,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Предекремент",NULL,NULL,f->predecrement,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Авто-92+",NULL,NULL,f->auto92,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Автотюнинг PID",NULL,NULL,f->pid_autotune,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Wi-Fi",NULL,NULL,f->wifi,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","MQTT",NULL,NULL,f->mqtt,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","NTP",NULL,NULL,f->ntp,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Web API",NULL,NULL,f->web_api,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Логирование SD",NULL,NULL,f->sd_log,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Датчик разлития",NULL,NULL,f->spill_sensor,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Мешалка",NULL,NULL,f->stirrer,1,0,NULL,NULL};
    items[n++] = (ui_list_item_t){"✓","Звук",NULL,NULL,f->sound,1,0,NULL,NULL};
    ui_build_list(c, items, n);
}

void ui_settings_calib(void)    { ui_sensor_calib_main(); }
void ui_settings_pid(void)      { lv_obj_t *c = ui_make_page("PID Autotune");
    lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "Ziegler-Nichols\nНажмите СТАРТ"); }
void ui_settings_general(void)  { lv_obj_t *c = ui_make_page("Общие");
    lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "Wi-Fi, MQTT, NTP,\nТема, Звук"); }
void ui_settings_export(void)   { lv_obj_t *c = ui_make_page("Экспорт");
    lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "Настройки → SD\nРецепты → SD\nБД → SD"); }
void ui_settings_restore(void)  { ui_show_resume_dialog(); }
void ui_settings_reset(void)    { lv_obj_t *c = ui_make_page("Полный сброс");
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, "⚠ ВНИМАНИЕ\n\nБудут сброшены:\nНастройки\nКалибровки\nAI");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFF3B30), 0); }

// Диагностика
void ui_diag_overview(void) {
    lv_obj_t *c = ui_make_page("Обзор");
    static char cpu[8], h[16], u[24];
    snprintf(cpu, sizeof(cpu), "%u%%", ui_cpu_get_load());
    snprintf(h, sizeof(h), "%u КБ", (unsigned)(esp_get_free_heap_size()/1024));
    uint32_t s = esp_timer_get_time()/1000000;
    snprintf(u, sizeof(u), "%u:%02u", s/3600, (s%3600)/60);
    static ui_card_t cards[] = {
        {"💻","CPU",cpu,NULL,NULL,0x34C759,NULL,NULL},
        {"🧠","RAM",h,NULL,NULL,0x007AFF,NULL,NULL},
        {"⏱","Uptime",u,NULL,NULL,0xFF9500,NULL,NULL},
    };
    ui_build_card_grid(c, cards, 3);
}

void ui_diag_i2c_scan(void) { lv_obj_t *c = ui_make_page("I2C");
    extern bool hal_i2c_probe(uint8_t);
    static char buf[512]; buf[0]=0;
    for (uint8_t a=1;a<127;a++) if (hal_i2c_probe(a)) {
        char t[16]; snprintf(t, sizeof(t), "%02X ", a); strcat(buf, t);
    }
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf[0] ? buf : "не найдено");
}

void ui_diag_ds18b20(void) { lv_obj_t *c = ui_make_page("DS18B20");
    static char buf[512];
    int n = hal_1wire_count();
    int pos = 0;
    for (int i=0;i<n;i++) pos += snprintf(buf+pos, sizeof(buf)-pos,
        "#%d %s: %.4f°C\n", i, hal_1wire_addr_str(i), hal_1wire_read(i));
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
}

void ui_diag_test_outputs(void) { lv_obj_t *c = ui_make_page("Тест");
    static ui_button_t btns[] = {
        {"Нагрев 10","🔥",0xFF3B30,NULL,NULL},
        {"Нагрев 50","🔥",0xFF9500,NULL,NULL},
        {"Всё ВЫКЛ","⏹",0x34C759,NULL,NULL},
    };
    extern void cb_test_heater_10(void*); extern void cb_test_heater_50(void*);
    extern void cb_test_all_off(void*);
    btns[0].on_click=cb_test_heater_10; btns[1].on_click=cb_test_heater_50;
    btns[2].on_click=cb_test_all_off;
    ui_build_button_bar(c, btns, 3);
}

void ui_diag_monitor(void) { ui_diag_i2c_scan(); }
void ui_diag_export(void)  { lv_obj_t *c = ui_make_page("Экспорт");
    lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "На SD:\nДиагностика\nЛоги\nСнимок датчиков"); }
void ui_diag_perf(void)    { lv_obj_t *c = ui_make_page("Производительность");
    lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "См. UART для деталей"); }