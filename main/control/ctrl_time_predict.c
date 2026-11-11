#include "ctrl_time_predict.h"
#include "core/events.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static time_prediction_t s_pred;
static uint32_t s_stage_start_ms = 0;
static float s_accumulated = 0;

void time_predict_init(void) {
    memset(&s_pred, 0, sizeof(s_pred));
    s_stage_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

void time_predict_reset(void) { time_predict_init(); }

void time_predict_set_stage_duration(uint32_t sec) {
    s_pred.total_sec = sec;
    s_pred.valid = true;
}

void time_predict_update(float temp, float target, float rate_per_min, float dt) {
    s_accumulated += dt;
    s_pred.elapsed_sec = (uint32_t)s_accumulated;

    if (fabsf(rate_per_min) > 0.001f) {
        float delta = target - temp;
        if (delta * rate_per_min > 0) {
            s_pred.remaining_sec = (uint32_t)(fabsf(delta / rate_per_min) * 60.0f);
        } else {
            s_pred.remaining_sec = 0;
        }
    } else {
        s_pred.remaining_sec = s_pred.total_sec > s_pred.elapsed_sec ?
                               (s_pred.total_sec - s_pred.elapsed_sec) : 0;
    }

    s_pred.total_sec = s_pred.elapsed_sec + s_pred.remaining_sec;
    s_pred.rate_per_min = rate_per_min;
    s_pred.valid = true;

    if (s_pred.total_sec > 0) {
        s_pred.progress_pct = (float)s_pred.elapsed_sec * 100.0f / s_pred.total_sec;
        if (s_pred.progress_pct > 100) s_pred.progress_pct = 100;
    }

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    int now_sec = tm_now->tm_hour * 3600 + tm_now->tm_min * 60 + tm_now->tm_sec;
    int eta_sec = now_sec + s_pred.remaining_sec;

    int day_offset = 0;
    while (eta_sec >= 86400) { eta_sec -= 86400; day_offset++; }

    s_pred.eta_hour = eta_sec / 3600;
    s_pred.eta_min  = (eta_sec % 3600) / 60;
    s_pred.eta_day_offset = day_offset;

    if (day_offset == 0)
        snprintf(s_pred.eta_str, sizeof(s_pred.eta_str), "%02d:%02d",
                 s_pred.eta_hour, s_pred.eta_min);
    else
        snprintf(s_pred.eta_str, sizeof(s_pred.eta_str), "%02d:%02d (+%d)",
                 s_pred.eta_hour, s_pred.eta_min, day_offset);
}

time_prediction_t* time_predict_get(void) { return &s_pred; }

const char* time_predict_eta_string(void) {
    return s_pred.valid ? s_pred.eta_str : "—";
}