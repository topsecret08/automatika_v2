#pragma once
#include <stdbool.h>

// Физический UI: статусные LED (питание/нагрев/авария) и buzzer.
//
// Дисплей/энкодер/тач уже обслуживаются существующим LVGL-стеком
// (hal_display/hal_touch/ui_*), поэтому здесь не дублируется инициализация
// ST7789. LED_*_GPIO == -1 означает «линия не распаяна»: модуль просто
// не трогает этот вывод.
//
// Buzzer работает через очередь: ui_physical_beep() никогда не блокирует
// safety/control-задачи.

void ui_physical_init(void);
void ui_physical_task(void *arg);

// Неблокирующий запрос звукового сигнала (0 — игнорировать).
void ui_physical_beep(int ms);

// Ручное управление статусными LED (например, из UI/API).
void ui_physical_set_led_power(bool on);
void ui_physical_set_led_heat(bool on);
void ui_physical_set_led_alarm(bool on);
