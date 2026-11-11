#include "ctrl_forecast.h"
#include <math.h>

static float s_rate = 0;
static float s_accel = 0;
static float s_last = 0;
static float s_last_rate = 0;

void ctrl_forecast_update(float temp, float dt) {
    if (dt <= 0) return;
    float inst = (temp - s_last) / dt;
    s_rate = 0.9f * s_rate + 0.1f * inst;

    float d_rate = (s_rate - s_last_rate) / dt;
    s_accel = 0.9f * s_accel + 0.1f * d_rate;
    s_last_rate = s_rate;
    s_last = temp;
}

float ctrl_forecast_rate(void) { return s_rate * 60.0f; }

float ctrl_forecast_predict(float sec) {
    return s_last + s_rate * sec + 0.5f * s_accel * sec * sec;
}

float ctrl_forecast_time_to(float target) {
    if (fabsf(s_rate) < 1e-5f) return 99999.0f;
    return (target - s_last) / s_rate;
}