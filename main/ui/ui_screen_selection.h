#pragma once

#include <stdbool.h>
#include "lvgl.h"

void ui_screen_selection_build(lv_obj_t *parent);
void ui_screen_selection_dashboard_create(void);

// Модальный редактор: энкодер перехватывает управление, пока окно открыто.
bool ui_screen_selection_modal_active(void);
bool ui_screen_selection_modal_rotate(int dir);
bool ui_screen_selection_modal_press(void);
