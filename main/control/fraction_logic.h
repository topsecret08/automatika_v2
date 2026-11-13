#pragma once

typedef enum {
    FRAC_HEADS = 0,
    FRAC_BODY,
    FRAC_TAILS,
    FRAC_DONE,
} fraction_t;

// Логика фракций (heads / body / tails).
//
// Переходы только вперёд; после сброса — снова HEADS. Пороги заданы в
// config.h (FRAC_*), температура — уже с поправкой на давление
// (baro_correct_boiling_point). Возвращается актуальная фракция.

void       fraction_reset(void);
fraction_t fraction_update(float t_corrected, float volume_ml);
fraction_t fraction_get(void);
const char *fraction_name(fraction_t f);
