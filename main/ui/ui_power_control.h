#pragma once

#include "lvgl.h"

// Компактный блок ручной/автоматической мощности:
// slider 0..100, статус, кнопка «Авто», ошибка неготового регулятора.
void ui_power_control_attach(lv_obj_t *parent);
