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

// Единая проверка блокировок нагрева. pct <= 0 всегда разрешено (это
// безопасное состояние). Отсутствующий датчик уровня не блокирует нагрев.
static inline bool heater_power_allowed(float pct, bool estop,
                                        bool safety_ok,
                                        bool level_available,
                                        bool level_too_low) {
    if (!(pct > 0.0f)) return true;
    if (estop || !safety_ok) return false;
    if (level_available && level_too_low) return false;
    return true;
}
