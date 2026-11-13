#pragma once
#include <stdbool.h>

void  ctrl_heater_init(void);

// Задание мощности 0..100 %. Единственная точка включения нагрева:
// внутри проверяются авария (ctrl_safety_ok) и E-STOP, а при росте
// температуры мощность ограничивается на HEATER_TREND_CUT.
void  ctrl_heater_set_percent(float pct);
// Внутренний выход в обход модульного гейта: только safety-задача,
// safety_gate и сервисный режим. Не вызывать из прикладного кода.
void  ctrl_heater_set_raw(float pct);
float ctrl_heater_get_percent(void);

// Сработал ли трендовый ограничитель на последнем вызове.
bool  ctrl_heater_trend_cut_active(void);

// Пишет в DAC без трендового ограничения (но не в обход безопасности).
bool  ctrl_heater_service_set(float pct);

void  ctrl_heater_task(void *arg);
