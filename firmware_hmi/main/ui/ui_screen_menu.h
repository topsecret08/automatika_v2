#pragma once

#include "lvgl.h"

typedef enum {
    UI_MENU_WORK = 0,
    UI_MENU_PREP,
    UI_MENU_DIAG,
    UI_MENU_SYS,
    UI_MENU_COUNT
} ui_menu_item_t;

/** on_select получает в user_data указатель на ui_menu_item_t. */
lv_obj_t *ui_screen_menu_create(lv_event_cb_t on_select);
void ui_screen_menu_set_active(ui_menu_item_t item);
