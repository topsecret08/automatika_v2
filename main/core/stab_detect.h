#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float   last_T;
    int64_t last_us;
    float   window_sec;
    float   tol_rate;
    float   band;
    float   stable_acc;
    float   max_sec;
    float   total_sec;
    bool    converged;
} stab_detector_t;

void stab_init(stab_detector_t *sd, float target_band, float tol_rate,
               float window_sec, float max_sec, int64_t now_us);
bool stab_tick(stab_detector_t *sd, float T, float T_target, int64_t now_us);
