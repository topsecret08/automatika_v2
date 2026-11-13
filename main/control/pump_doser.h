#pragma once
#include <stdint.h>
#include "esp_err.h"

// Публичный фасад дозатора (совместимый с ТЗ V0.5 alfa).
//
// Реальная работа с STEP/ENABLE и очередью команд живёт в control/ctrl_dose.c,
// калибровка steps_per_ml — в core/control_config.c (NVS "automatika").
// Этот модуль не дублирует драйвер, а даёт стабильный API:
//   * pump_calib_load/save — чтение/запись калибровки;
//   * pump_dose_volume     — постановка дозы в очередь doser (не блокирует
//     вызывающий поток, поэтому безопасен для HTTP-обработчика).
//
// Ограничения:
//   * E-STOP / авария / отсутствие service-режима проверяются внутри
//     ctrl_dose_volume();
//   * объём ограничен DOSER_MAX_DOSE_ML (2000 мл), доза сверх лимита
//     отклоняется с ESP_ERR_INVALID_SIZE.

typedef struct {
    uint32_t steps_per_ml;
    uint32_t max_steps;     // DOSER_MAX_DOSE_ML * steps_per_ml
} PumpCalib;

#define PUMP_STEPS_PER_ML_DEFAULT 1000u

esp_err_t pump_calib_load(PumpCalib *c);
esp_err_t pump_calib_save(const PumpCalib *c);

// ESP_OK               — доза поставлена в очередь;
// ESP_ERR_INVALID_ARG  — некорректный объём;
// ESP_ERR_INVALID_SIZE — объём больше max_steps;
// ESP_ERR_INVALID_STATE— авария/E-STOP/очередь занята.
esp_err_t pump_dose_volume(float volume_ml, float rate_ml_per_min);
