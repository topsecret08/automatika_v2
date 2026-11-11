#pragma once
#include <stdint.h>
typedef struct {
    const char *name;
    float stop_temp;
    float body_rate_ml_min;
} distill_preset_t;

const distill_preset_t* distill_preset_get(int idx);
int distill_preset_count(void);