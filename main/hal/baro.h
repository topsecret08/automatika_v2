#pragma once
#include "esp_err.h"
#include <stdbool.h>

// BMP280 (I2C 0x77/0x76) — атмосферное давление и коррекция точки кипения.
//
// Драйвер реализован на чистом C поверх hal_i2c (без managed-компонента):
// читаются калибровочные коэффициенты 0x88..0xA1 и выполняется штатная
// компенсация Bosch. Измерение — forced mode, опрос раз в BARO_POLL_MS.
//
// Если датчик не найден, baro_available() == false, а baro_get_pressure_hpa()
// возвращает STANDARD_ATM_HPA (1013.25 гПа). Все потребители обязаны
// воспринимать это как «нет данных», а не как измерение.

esp_err_t baro_init(void);
void      baro_task(void *arg);

bool  baro_available(void);
float baro_get_pressure_hpa(void);
float baro_get_temperature_c(void);

// Точка кипения воды при данном давлении, °C (100 °C при 1013.25 гПа).
float baro_boiling_point_c(float p_hpa);

// Поправка измеренной температуры к нормальному давлению:
//   t_corr = t_measured + (1013.25 - p_hpa) * BOIL_CORRECTION_COEFF
// Нужна, чтобы пороги фракций (заданные для 1013.25 гПа) не «уезжали»
// в горах или при заметном изменении погоды.
float baro_correct_boiling_point(float t_measured, float p_hpa);
