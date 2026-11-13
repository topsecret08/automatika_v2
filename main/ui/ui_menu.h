#pragma once

#include "lvgl.h"

// Иерархическое меню физического LCD.
//
// 4 группы по задачам пользователя (iOS HIG): Работа, Подготовка,
// Диагностика, Система. Внутри — не более 4 пунктов.
//
// Экран рисуется в ui.c (там живёт g_screen/clear_screen), а деревья
// и действия — здесь. Энкодер опрашивается в ui_physical.c и вызывает
// ui_menu_encoder_*; на текущей плате пины ENCODER_* = -1, поэтому
// навигация доступна тапом по экрану.

typedef struct ui_menu_item {
    const char *label;
    void (*on_enter)(void);
    struct ui_menu_item *children;
    int children_count;
} ui_menu_item;

extern ui_menu_item menu_work[];
extern ui_menu_item menu_prepare[];
extern ui_menu_item menu_diagnose[];
extern ui_menu_item menu_system[];

// Реализованы в ui.c: очищают g_screen и вызывают build_*.
void ui_menu_show_root(int selected_tab);
void ui_menu_show_group(int tab, int selected_item);

// Реализованы в ui_menu.c.
void ui_menu_build_root(lv_obj_t *parent, int selected_tab);
void ui_menu_build_group(lv_obj_t *parent, int tab, int selected_item);
int  ui_menu_tab_count(void);
const char *ui_menu_tab_label(int tab);

// Синхронизирует внутреннее состояние (нужно при входе из тача/меню).
void ui_menu_set_state(int tab, int level, int item);

// События энкодера. dir: -1 назад, +1 вперёд.
void ui_menu_encoder_rotate(int dir);
void ui_menu_encoder_press(void);
void ui_menu_encoder_long_press(void);

// Жесты/свайпы LCD.
void ui_menu_next_tab(void);
void ui_menu_prev_tab(void);
void ui_menu_enter(void);
void ui_menu_back(void);
int  ui_menu_current_tab(void);
int  ui_menu_current_level(void);
