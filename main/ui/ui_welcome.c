#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/events.h"
#include "core/roles.h"
#include "core/flags.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

extern lv_obj_t* ui_make_page(const char *title);

typedef enum {
    WIZ_LANG = 0,
    WIZ_HELLO,
    WIZ_CHECK,
    WIZ_PIN,
    WIZ_WIFI,
    WIZ_DONE,
} wizard_step_t;

static wizard_step_t s_step = WIZ_LANG;
static uint16_t s_new_pin = 0;

static void wizard_next(void);

// Шаг 1: Выбор языка
static void show_lang_step(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    lv_scr_load(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Выберите язык");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    static ui_button_t btns[] = {
        {"Русский", "🇷🇺", 0x007AFF, NULL, NULL},
        {"English", "🇬🇧", 0x34C759, NULL, NULL},
        {"Deutsch", "🇩🇪", 0xFF9500, NULL, NULL},
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 300, 60);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, -60 + i * 80);
        ui_style_apply_btn_primary(btn);
        lv_obj_t *l = lv_label_create(btn);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  %s", btns[i].icon, btns[i].text);
        lv_label_set_text(l, buf);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            (void)e;
            s_step = WIZ_HELLO;
            wizard_next();
        }, LV_EVENT_CLICKED, NULL);
    }
}

// Шаг 2: Приветствие
static void show_hello_step(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    lv_scr_load(scr);

    lv_obj_t *t1 = lv_label_create(scr);
    lv_label_set_text(t1, "АВТОМАТИКА v2.9");
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(t1, IOS_BLUE, 0);
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *t2 = lv_label_create(scr);
    lv_label_set_text(t2, "Что будем сегодня делать?");
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_18, 0);
    lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 80);

    const char *modes[] = {"🥃 Дистилляция", "⚗ Ректификация", "🍺 Пивоварня",
                          "🌡 Брожение", "🔬 Диагностика"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 300, 44);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 120 + i * 50);
        ui_style_apply_card(btn);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, modes[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            (void)e;
            s_step = WIZ_CHECK;
            wizard_next();
        }, LV_EVENT_CLICKED, NULL);
    }
}

// Шаг 3: Проверка системы
static void show_check_step(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    lv_scr_load(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Проверка системы...");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    const char *checks[] = {"I2C шина", "Датчики DS18B20", "SD-карта",
                            "Дисплей", "Реле", "Весы"};
    extern bool hal_i2c_probe(uint8_t);
    extern int hal_1wire_count(void);
    extern bool storage_sd_ready(void);

    int results[6];
    results[0] = hal_i2c_probe(0x48) ? 1 : 0;
    results[1] = hal_1wire_count() > 0 ? 1 : 0;
    results[2] = storage_sd_ready() ? 1 : 0;
    results[3] = 1;
    results[4] = hal_i2c_probe(0x11) ? 1 : 0;
    results[5] = 1;

    for (int i = 0; i < 6; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 350, 40);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 70 + i * 45);
        ui_style_apply_card(row);

        lv_obj_t *l = lv_label_create(row);
        lv_label_set_text(l, checks[i]);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *s = lv_label_create(row);
        lv_label_set_text(s, results[i] ? "✓ OK" : "✗ FAIL");
        lv_obj_set_style_text_color(s, results[i] ? IOS_GREEN : IOS_RED, 0);
        lv_obj_align(s, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 200, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    ui_style_apply_btn_primary(btn);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, "Далее →");
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        (void)e;
        s_step = WIZ_PIN;
        wizard_next();
    }, LV_EVENT_CLICKED, NULL);
}

// Шаг 4: PIN админа
static void show_pin_step(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    lv_scr_load(scr);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "PIN администратора");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "По умолчанию: 3333\nМожно изменить позже");
    ui_style_apply_subtitle(hint);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 70);

    lv_obj_t *ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, "3333");
    lv_textarea_set_max_length(ta, 4);
    lv_obj_set_size(ta, 250, 60);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 200, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    ui_style_apply_btn_primary(btn);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, "✓ Сохранить");
    lv_obj_center(l);

    static lv_obj_t *s_ta = NULL;
    s_ta = ta;
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        (void)e;
        extern void roles_change_pin(int, uint16_t);
        const char *txt = lv_textarea_get_text(s_ta);
        uint16_t pin = (uint16_t)atoi(txt);
        if (pin >= 1000 && pin <= 9999)
            roles_change_pin(3, pin);
        s_step = WIZ_WIFI;
        wizard_next();
    }, LV_EVENT_CLICKED, NULL);
}

// Шаг 5: Wi-Fi
static void show_wifi_step(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    lv_scr_load(scr);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "Настройка Wi-Fi");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *l1 = lv_label_create(scr);
    lv_label_set_text(l1, "SSID:");
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 30, 90);

    lv_obj_t *ta1 = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta1, true);
    lv_obj_set_size(ta1, 380, 44);
    lv_obj_align(ta1, LV_ALIGN_TOP_MID, 0, 120);

    lv_obj_t *l2 = lv_label_create(scr);
    lv_label_set_text(l2, "Пароль:");
    lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 30, 180);

    lv_obj_t *ta2 = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta2, true);
    lv_textarea_set_password_mode(ta2, true);
    lv_obj_set_size(ta2, 380, 44);
    lv_obj_align(ta2, LV_ALIGN_TOP_MID, 0, 210);

    static lv_obj_t *s_ssid = NULL, *s_pass = NULL;
    s_ssid = ta1;
    s_pass = ta2;

    static ui_button_t btns[] = {
        {"Пропустить", "→", 0x8E8E93, NULL, NULL},
        {"Сохранить", "✓", 0x34C759, NULL, NULL},
    };
    btns[0].on_click = [](void*) {
        s_step = WIZ_DONE;
        wizard_next();
    };
    btns[1].on_click = [](void*) {
        extern void flags_save(void);
        // Сохраняем в NVS
        nvs_handle_t h;
        nvs_open("wifi_cfg", NVS_READWRITE, &h);
        nvs_set_str(h, "ssid", lv_textarea_get_text(s_ssid));
        nvs_set_str(h, "pass", lv_textarea_get_text(s_pass));
        nvs_commit(h);
        nvs_close(h);
        s_step = WIZ_DONE;
        wizard_next();
    };
    ui_build_button_bar(scr, btns, 2);
}

// Шаг 6: Готово
static void show_done_step(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    lv_scr_load(scr);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "✓ Готово!");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(t, IOS_GREEN, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -60);

    lv_obj_t *h = lv_label_create(scr);
    lv_label_set_text(h,
        "Система настроена.\n\n"
        "Далее:\n"
        "• Калибровка датчиков\n"
        "• Калибровка клапанов\n"
        "• Первый процесс");
    lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 250, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    ui_style_apply_btn_primary(btn);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, "Начать работу");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        (void)e;
        events_log(EVENT_INFO, "Wizard complete");
        ui_main_menu();
    }, LV_EVENT_CLICKED, NULL);
}

static void wizard_next(void) {
    switch (s_step) {
    case WIZ_LANG:  show_lang_step(); break;
    case WIZ_HELLO: show_hello_step(); break;
    case WIZ_CHECK: show_check_step(); break;
    case WIZ_PIN:   show_pin_step(); break;
    case WIZ_WIFI:  show_wifi_step(); break;
    case WIZ_DONE:  show_done_step(); break;
    }
}

void ui_show_welcome_ex(void) {
    s_step = WIZ_LANG;
    wizard_next();
}