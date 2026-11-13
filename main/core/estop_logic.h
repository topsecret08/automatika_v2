#pragma once

#include <stdbool.h>
#include "config.h"

// Чистая проверка полярности E-STOP: для NC-кнопки активный уровень —
// высокий (контакт разомкнут, pull-up). Низкий — норма.
static inline bool estop_level_active(int level) {
    return level == ESTOP_ACTIVE_LEVEL;
}
