#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

// Минимальные заглушки для unit-тестов.
//
// events.c обращается к SD только для дублирования журнала — в тестах
// файловая система не нужна.
bool storage_sd_ready(void) { return false; }

// pump_doser.c — фасад над control_config/ctrl_dose. В тестах проверяется
// только его собственная логика (значения по умолчанию и отказ до
// постановки в очередь), поэтому конфигурация и очередь заменены заглушками.
uint32_t control_config_get_steps_per_ml(void) { return 1000u; }

esp_err_t control_config_set_steps_per_ml(uint32_t steps) {
    (void)steps;
    return ESP_OK;
}

float control_config_get_rate_ml_min(void) { return 60.0f; }

bool ctrl_dose_volume(float volume_ml, float rate_ml_per_min) {
    (void)volume_ml;
    (void)rate_ml_per_min;
    return true;
}
