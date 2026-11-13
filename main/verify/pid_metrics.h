#pragma once

#include "esp_err.h"

#define PID_OVERSHOOT_MAX   5.0f
#define PID_SETTLING_MAX    120.0f
#define PID_SSE_MAX         2.0f
#define PID_OSC_MAX         0.10f

typedef struct {
    float overshoot_pct;
    float settling_s;
    float sse;
    float osc_index;
} pid_metrics_t;

// sp/y — массивы одинаковой длины; dt — шаг между отсчётами, с.
// step_value — амплитуда ступеньки уставки.
esp_err_t pid_compute_metrics(const float *sp, const float *y, int n,
                              float dt, float step_value, pid_metrics_t *out);
