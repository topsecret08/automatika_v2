#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Давление в колонне.
//
// Основной источник — BMP280 (baro): давление считается как разность
// текущего абсолютного давления и «нуля», снятого при старте/по команде.
// Если BMP280 нет, используется аналоговый датчик 4–20 мА (hal_adc).
//
//   column_pressure_kpa() >= 0; аварийный порог SAFE_MAX_PRESSURE*100 кПа.

esp_err_t pressure_sensor_init(void);
void      pressure_sensor_rezero(void);

float column_pressure_kpa(void);
float column_pressure_bar(void);

// Диагностика для API: какой источник используется.
const char *pressure_sensor_source(void);
