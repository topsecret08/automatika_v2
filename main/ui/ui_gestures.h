#pragma once

#include <stdbool.h>
#include <stdint.h>

// Свайпы на LCD. Драйвер тача (FT6336) передаёт сюда каждое касание;
// модуль сам считает расстояние/скорость, поэтому не зависит от того,
// по какому LVGL-объекту пришлось касание.
//
// Порог: 50 px и 0.3 px/мс (как в UI guidelines).
void ui_gestures_feed(int16_t x, int16_t y, bool pressed);

// Выполняет отложенное действие свайпа. Вызывать из задачи UI под
// hal_display_lock(); менять экран прямо в read_cb тача нельзя.
void ui_gestures_process(void);
