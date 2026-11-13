#pragma once

#include "lvgl.h"

// Экран дашборда в стиле glass для LCD 480x320.
//
// build() рисует содержимое в готовый parent (g_screen из ui.c).
// create() объявлен здесь, но реализован в ui.c: ему нужен доступ к
// статическому clear_screen(), чтобы не оставить висячий g_screen.
void ui_screen_home_build(lv_obj_t *parent);
void ui_screen_home_create(void);
