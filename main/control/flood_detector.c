#include "flood_detector.h"
#include <math.h>

#define FLOOD_P_HPA      200.0f
#define CHOKE_P_HPA      350.0f
#define HOLD_S           30.0f

void flood_detector_init(flood_detector_t *fd) {
    if (!fd) return;
    fd->flooded = false;
    fd->choke = false;
    fd->hold_s = 0.0f;
    fd->score = 0.0f;
    fd->last_pressure_hpa = NAN;
}

void flood_detector_tick(flood_detector_t *fd, const flood_input_t *in) {
    if (!fd || !in || !isfinite(in->dt_s) || in->dt_s <= 0.0f) return;

    bool p_ok = isfinite(in->pressure_hpa) && in->pressure_hpa > -900.0f;
    float p = p_ok ? in->pressure_hpa : 0.0f;
    float dp = (p_ok && isfinite(fd->last_pressure_hpa)) ? (p - fd->last_pressure_hpa) / in->dt_s : 0.0f;
    if (p_ok) fd->last_pressure_hpa = p;

    float score = 0.0f;
    if (p >= CHOKE_P_HPA) score += 2.0f;
    else if (p >= FLOOD_P_HPA) score += 1.0f;
    if (dp > 0.05f) score += 0.5f;
    if (isfinite(in->cube_c) && isfinite(in->column_c) && in->cube_c > 90.0f && (in->cube_c - in->column_c) > 20.0f)
        score += 0.5f;
    if (isfinite(in->power_pct) && in->power_pct > 85.0f && p >= FLOOD_P_HPA) score += 0.5f;

    fd->score = score;
    if (score >= 2.0f) { fd->flooded = true; fd->choke = true; fd->hold_s = HOLD_S; }
    else if (score >= 1.0f) { fd->flooded = true; fd->choke = false; fd->hold_s = HOLD_S; }

    if (fd->hold_s > 0.0f) {
        fd->hold_s -= in->dt_s;
        if (fd->hold_s <= 0.0f && score < 1.0f) {
            fd->flooded = false; fd->choke = false; fd->hold_s = 0.0f;
        }
    } else if (score < 1.0f) {
        fd->flooded = false; fd->choke = false;
    }
}

bool flood_detector_active(const flood_detector_t *fd) { return fd && fd->flooded; }
const char *flood_detector_reason(const flood_detector_t *fd) {
    if (!fd || !fd->flooded) return "none";
    return fd->choke ? "choke" : "flood";
}
void flood_detector_reset(flood_detector_t *fd) { flood_detector_init(fd); }
