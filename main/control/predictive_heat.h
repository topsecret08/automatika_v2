#pragma once
#include <stdbool.h>

typedef struct {
    float target_c;
    float current_c;
    float rate_c_min;
    float dt_s;
    float max_power_pct;
} pred_heat_in_t;

typedef struct {
    float predicted_c;
    float cap_pct;
    bool  active;
} pred_heat_out_t;

void predictive_heat_init(void);
pred_heat_out_t predictive_heat_limit(const pred_heat_in_t *in);
