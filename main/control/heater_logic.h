#pragma once

#include <stdbool.h>

// Чистая логика приоритета ручной/автоматической уставки.
static inline float heater_effective_pct(bool manual, float manual_pct,
                                         float auto_pct) {
    float v = manual ? manual_pct : auto_pct;
    if (!(v > 0.0f)) return 0.0f;
    if (v > 100.0f) return 100.0f;
    return v;
}
