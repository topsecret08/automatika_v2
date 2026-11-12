#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/roles.h"
#include <stdio.h>
#include <stdlib.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static lv_obj_t *s_pin_ta = NULL;
static lv_obj_t *s_new_pin_ta = NULL;

void ui_roles_login_dialog(void);

static void cb_roles_login(void *c) {
    (void)c;
    uint16_t pin = (uint16_t)atoi(lv_textarea_get_text(s_pin_ta));
    if (roles_login(pin)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Роль: %s", roles_name(roles_get_current()));
        ui_toast(buf);
        ui_main_menu();
    } else {
        ui_toast("Неверный PIN");
    }
}

static void cb_roles_cancel(void *c) {
    (void)c;
    ui_main_menu();
}

static void cb_roles_change(void *c) {
    (void)c;
    ui_roles_login_dialog();
}

static void cb_roles_logout(void *c) {
    (void)c;
    roles_logout();
    ui_toast("Выход");
    ui_main_menu();
}

static void cb_roles_timeout(void *ctx) {
    uint16_t v = (uint16_t)(intptr_t)ctx;
    if (v != 0xFFFF) roles_set_timeout(v);
    ui_toast("Сохранено");
}

static void cb_roles_save_pin(void *c) {
    (void)c;
    uint16_t pin = (uint16_t)atoi(lv_textarea_get_text(s_new_pin_ta));
    if (pin >= 1000 && pin <= 9999) {
        roles_change_pin(ROLE_ADMIN, pin);
        ui_toast("PIN изменён");
    } else {
        ui_toast("4 цифры, от 1000 до 9999");
    }
}

void ui_roles_login_dialog(void) {
    lv_obj_t *scr = ui_make_page("Вход по PIN");

    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l, "Введите PIN:");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, 4);
    lv_obj_set_size(ta, 200, 60);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);
    s_pin_ta = ta;

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint,
        "1111 — Оператор\n2222 — Инженер\n3333 — Админ");
    ui_style_apply_subtitle(hint);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 60);

    static ui_button_t btns[] = {
        {"Войти", "✓", 0x34C759, NULL, NULL},
        {"Отмена", "✕", 0xFF3B30, NULL, NULL},
    };
    btns[0].on_click = cb_roles_login;
    btns[1].on_click = cb_roles_cancel;
    ui_build_button_bar(scr, btns, 2);
}

void ui_roles_current(void) {
    lv_obj_t *scr = ui_make_page("Текущая роль");

    lv_obj_t *l = lv_label_create(scr);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "Роль: %s\n\nДоступно:\n"
        "• Просмотр     %s\n"
        "• Графики      %s\n"
        "• Логи         %s\n"
        "• Запуск       %s\n"
        "• Настройки    %s\n"
        "• Калибровка   %s\n"
        "• Прошивка     %s",
        roles_name(roles_get_current()),
        roles_check(ROLE_GUEST)    ? "✓" : "✗",
        roles_check(ROLE_GUEST)    ? "✓" : "✗",
        roles_check(ROLE_GUEST)    ? "✓" : "✗",
        roles_check(ROLE_OPERATOR) ? "✓" : "✗",
        roles_check(ROLE_ENGINEER) ? "✓" : "✗",
        roles_check(ROLE_ENGINEER) ? "✓" : "✗",
        roles_check(ROLE_ADMIN)    ? "✓" : "✗");
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

    static ui_button_t btns[] = {
        {"Сменить", "🔄", 0x007AFF, NULL, NULL},
        {"Выйти", "🚪", 0xFF3B30, NULL, NULL},
    };
    btns[0].on_click = cb_roles_change;
    btns[1].on_click = cb_roles_logout;
    ui_build_button_bar(scr, btns, 2);
}

void ui_roles_matrix_screen(void) {
    lv_obj_t *scr = ui_make_page("Матрица прав");

    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l,
        "Действие             Роль\n"
        "────────────────────────\n"
        "Просмотр             Гость\n"
        "Графики              Гость\n"
        "Логи                 Гость\n"
        "Справка              Гость\n"
        "Запуск процесса      Оператор\n"
        "Остановка            Оператор\n"
        "Сброс аварии         Оператор\n"
        "Смена рецепта        Оператор\n"
        "Ручное управление    Оператор\n"
        "Уставки              Инженер\n"
        "Калибровка           Инженер\n"
        "Автотюнинг           Инженер\n"
        "Настройки            Инженер\n"
        "Прошивка             Админ\n"
        "Заводские            Админ\n"
        "Управление ролями    Админ");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);
}

void ui_roles_timeout_dialog(void) {
    lv_obj_t *scr = ui_make_page("Автовыход");

    const char *opts[] = {"5 минут", "10 минут", "30 минут", "Никогда"};
    const uint16_t vals[] = {300, 600, 1800, 0xFFFF};

    static ui_list_item_t items[4];
    for (int i = 0; i < 4; i++) {
        items[i] = (ui_list_item_t){
            .icon = "⏱", .title = opts[i],
            .on_click = cb_roles_timeout,
            .ctx = (void*)(intptr_t)vals[i]
        };
    }
    ui_build_list(scr, items, 4);
}

void ui_roles_change_pin(void) {
    lv_obj_t *scr = ui_make_page("Изменить PIN");

    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l, "Введите новый PIN администратора:");
    ui_style_apply_subtitle(l);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 4);
    lv_obj_set_size(ta, 200, 60);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);

    s_new_pin_ta = ta;

    static ui_button_t btns[] = {
        {"✓ Сохранить", NULL, 0x34C759, NULL, NULL},
    };
    btns[0].on_click = cb_roles_save_pin;
    ui_build_button_bar(scr, btns, 1);
}