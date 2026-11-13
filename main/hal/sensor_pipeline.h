#pragma once

#include <stdbool.h>
#include <stdint.h>

// Конвейер обработки температуры:
//   1) валидация (85 °C / -127 °C и диапазон);
//   2) медианный фильтр по 5 отсчётам;
//   3) одномерный Калман со сглаживанием и оценкой тренда;
//   4) приведение к 760 мм рт. ст.
//
// Модуль не читает железо: ему передают сырое значение и давление.
// Один экземпляр на канал.

typedef struct {
    float buf[5];
    int   idx, cnt;

    float x_est, p_est;
    float q, r;
    float trend;          // °C/мин
    float last_x;
    int64_t last_t;

    float t_corrected;
    float raw;
    bool  valid;
} TempPipeline;

void  temp_pipeline_init(TempPipeline *p, float q, float r);

// Возвращает скомпенсированную температуру или NAN, если значение невалидно.
float temp_pipeline_update(TempPipeline *p, float raw, float pressure_hpa);

// Скомпенсировать последнюю оценку без нового отсчёта.
float temp_pipeline_compensate(const TempPipeline *p, float pressure_hpa);

float temp_pipeline_get_trend(const TempPipeline *p);      // °C/мин
float temp_pipeline_filtered(const TempPipeline *p);       // без компенсации
bool  temp_pipeline_is_stable(const TempPipeline *p, float threshold_c_per_min);
bool  temp_pipeline_is_valid(const TempPipeline *p);
