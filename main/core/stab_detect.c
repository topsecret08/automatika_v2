#include "stab_detect.h"
#include <math.h>
#include <stddef.h>

void stab_init(stab_detector_t *sd, float target_band, float tol_rate,
               float window_sec, float max_sec, int64_t now_us) {
    if (!sd) return;
    *sd = (stab_detector_t){
        .last_T = NAN,
        .last_us = now_us,
        .window_sec = window_sec > 0.0f ? window_sec : 60.0f,
        .tol_rate = tol_rate > 0.0f ? tol_rate : 0.01f,
        .band = target_band > 0.0f ? target_band : 0.1f,
        .max_sec = max_sec > 0.0f ? max_sec : 900.0f,
    };
}

bool stab_tick(stab_detector_t *sd, float T, float T_target, int64_t now_us) {
    if (!sd || !isfinite(T)) return false;
    if (sd->converged) return true;

    float dt = (float)(now_us - sd->last_us) / 1e6f;
    if (dt < 0.0f) dt = 0.0f;
    sd->total_sec += dt;
    sd->last_us = now_us;

    if (sd->total_sec >= sd->max_sec) {
        sd->converged = true;
        return true;
    }

    if (!isfinite(sd->last_T)) {
        sd->last_T = T;
        return false;
    }

    float rate = fabsf(T - sd->last_T) / (dt > 0.01f ? dt : 0.01f);
    sd->last_T = T;

    bool in_band = fabsf(T - T_target) < sd->band;
    if (in_band && rate < sd->tol_rate) sd->stable_acc += dt;
    else sd->stable_acc = 0.0f;

    if (sd->stable_acc >= sd->window_sec) {
        sd->converged = true;
        return true;
    }
    return false;
}
