#pragma once

#include <stdbool.h>

// Учёт объёма по фактическому весу, если HX711 откалиброван.
// Если весы недоступны, режимы используют расчётную оценку по расходу.
typedef struct {
    float base_ml;
    bool  active;
} volume_feedback_t;

void  volume_feedback_begin(volume_feedback_t *v);
// Возвращает измеренный объём с начала этапа или -1, если весы недоступны.
float volume_feedback_ml(const volume_feedback_t *v);
