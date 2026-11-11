#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t elapsed_sec;
    uint32_t remaining_sec;
    uint32_t total_sec;
    float    progress_pct;
    uint8_t  eta_hour;
    uint8_t  eta_min;
    uint8_t  eta_day_offset;
    char     eta_str[32];
    bool     valid;
    float    rate_per_min;
} time_prediction_t;

void  time_predict_init(void);
void  time_predict_update(float temp, float target, float rate_per_min, float dt);
time_prediction_t* time_predict_get(void);
void  time_predict_set_stage_duration(uint32_t sec);
void  time_predict_reset(void);
const char* time_predict_eta_string(void);