#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Управление флегмовым числом (reflux ratio).
//
// Клапан — сервопривод PCA9685, канал REFLUX_VALVE_CHANNEL (3), поэтому
// каналы 0..2 (головы/тело/хвосты) не затрагиваются.
//
// Период 1 с; клапан открыт долю 1/(1+ratio):
//   ratio 0  -> открыт всегда (100 %)
//   ratio 3  -> 25 % периода
//   ratio 10 -> ~9 %
//
// Вызов reflux_update() каждые 100 мс (reflux_task делает это сам).
// ratio == 0 и disabled -> клапан закрыт.

esp_err_t reflux_init(float ratio);
void      reflux_task(void *arg);

void  reflux_set_ratio(float ratio);
float reflux_get_ratio(void);

void  reflux_enable(bool on);
bool  reflux_enabled(void);

// Один шаг ШИМ; вызывается задачей, но безопасен и из цикла управления.
void  reflux_update(void);
