#pragma once

#include <stdbool.h>
#include "lvgl.h"

void ui_screen_ssvc_build(lv_obj_t *parent);
void ui_screen_ssvc_dashboard_create(void);

// Модальный редактор: энкодер перехватывает управление, пока окно открыто.
bool ui_screen_ssvc_modal_active(void);
bool ui_screen_ssvc_modal_rotate(int dir);
bool ui_screen_ssvc_modal_press(void);
