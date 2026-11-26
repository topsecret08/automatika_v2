#pragma once

#include <stdbool.h>
#include <stdint.h>

// Обратная связь по весам/объёму. HX711 остаётся индикатором,
// но при наличии надёжных показаний даёт замкнутый контур:
// целевой объём, фактический поток, детект отсутствия потока и перелива.
typedef struct {
    float    base_ml;
    bool     active;
    float    target_ml;
    float    overflow_ml;
    float    last_ml;
    uint32_t last_progress_ms;
    uint32_t no_flow_timeout_ms;
    bool     no_flow;
    bool     overflow;
} volume_feedback_t;

void  volume_feedback_begin(volume_feedback_t *v);
void  volume_feedback_set_target(volume_feedback_t *v, float target_ml, float overflow_ml, uint32_t no_flow_timeout_ms);
// Возвращает накопленный объём мл от начала сессии или -1, если весы недоступны.
float volume_feedback_ml(const volume_feedback_t *v);
// Обновляет детекты no-flow/overflow. Вызывать периодически при активном отборе.
void  volume_feedback_tick(volume_feedback_t *v, bool flow_expected, uint32_t now_ms);
bool  volume_feedback_target_reached(const volume_feedback_t *v);
bool  volume_feedback_no_flow(const volume_feedback_t *v);
bool  volume_feedback_overflow(const volume_feedback_t *v);
