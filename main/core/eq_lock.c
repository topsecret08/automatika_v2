#include "eq_lock.h"
#include "event_bus.h"
#include <math.h>
#include <stddef.h>

#define EQ_RAMP_SEC_DEFAULT   120.0f
#define EQ_MIN_POWER_DEFAULT    5.0f
#define EQ_RATE_TOL_DEFAULT     0.02f
#define EQ_STABLE_SEC_DEFAULT  60.0f

void eq_lock_init(eq_lock_t *eq) {
    if (!eq) return;
    *eq = (eq_lock_t){
        .phase = EQ_IDLE,
        .ramp_sec = EQ_RAMP_SEC_DEFAULT,
        .min_power = EQ_MIN_POWER_DEFAULT,
        .rate_tol = EQ_RATE_TOL_DEFAULT,
        .stable_sec = EQ_STABLE_SEC_DEFAULT,
    };
}

bool eq_lock_engage(eq_lock_t *eq, float current_power_pct,
                    uint32_t timeout_sec, int64_t now_us) {
    if (!eq || eq->phase != EQ_IDLE) return false;
    eq->phase = EQ_PREPARE;
    eq->phase_start_us = now_us;
    eq->power_start = current_power_pct;
    eq->power_current = current_power_pct;
    eq->timeout_sec = timeout_sec;
    eq->release_pending = false;
    eq->stable_acc_sec = 0.0f;
    return true;
}

bool eq_lock_tick(eq_lock_t *eq, const eq_lock_input_t *in,
                  float dt, int64_t now_us) {
    if (!eq || !in) return false;
    if (dt < 0.0f || !isfinite(dt)) dt = 0.0f;

    switch (eq->phase) {
    case EQ_IDLE:
        return true;

    case EQ_PREPARE:
        if (!in->body_valve_closed || !in->safety_ok) return false;
        eq->phase = EQ_RAMP_DOWN;
        eq->phase_start_us = now_us;
        eq->power_start = eq->power_current;
        break;

    case EQ_RAMP_DOWN: {
        float elapsed = (float)(now_us - eq->phase_start_us) / 1e6f;
        float k = elapsed / (eq->ramp_sec > 0.0f ? eq->ramp_sec : 1.0f);
        if (k >= 1.0f) {
            eq->power_current = eq->min_power;
            eq->phase = EQ_EQUILIBRATE;
            eq->stable_acc_sec = 0.0f;
            eq->last_cube = in->cube_c;
            eq->last_col = in->column_c;
            eq->last_sample_us = now_us;
        } else {
            eq->power_current = eq->power_start * (1.0f - k) +
                                eq->min_power * k;
        }
        break;
    }

    case EQ_EQUILIBRATE: {
        float sdt = (float)(now_us - eq->last_sample_us) / 1e6f;
        if (sdt < 0.5f) break;
        float rc = fabsf(in->cube_c - eq->last_cube) / sdt;
        float rl = fabsf(in->column_c - eq->last_col) / sdt;
        eq->last_cube = in->cube_c;
        eq->last_col = in->column_c;
        eq->last_sample_us = now_us;
        if (rc < eq->rate_tol && rl < eq->rate_tol) {
            eq->stable_acc_sec += sdt;
        } else {
            eq->stable_acc_sec = 0.0f;
        }
        if (eq->stable_acc_sec >= eq->stable_sec) {
            eq->phase = EQ_LOCKED;
            eq->phase_start_us = now_us;
            bus_event_t ev = { EV_MAJOR, "EQ_LOCK_ENGAGED", 0,
                               "thermal equilibrium locked", now_us };
            event_bus_publish(&ev);
        }
        break;
    }

    case EQ_LOCKED:
        if (eq->release_pending ||
            (eq->timeout_sec > 0 &&
             (uint64_t)(now_us - eq->phase_start_us) / 1000000ULL >=
                 eq->timeout_sec)) {
            eq->phase = EQ_RELEASE;
            eq->release_pending = true;
        }
        break;

    case EQ_RELEASE:
        if (!in->safety_ok) return false;
        if (eq->release_pending) {
            eq->phase = EQ_IDLE;
            eq->release_pending = false;
            return true;
        }
        return false;

    default:
        return false;
    }
    return true;
}

bool eq_lock_release(eq_lock_t *eq) {
    if (!eq) return false;
    if (eq->phase == EQ_IDLE) return true;
    eq->release_pending = true;
    eq->phase = EQ_RELEASE;
    return true;
}

eq_phase_t eq_lock_phase(const eq_lock_t *eq) {
    return eq ? eq->phase : EQ_IDLE;
}

const char *eq_lock_phase_name(eq_phase_t phase) {
    switch (phase) {
    case EQ_PREPARE:     return "prepare";
    case EQ_RAMP_DOWN:   return "ramp_down";
    case EQ_EQUILIBRATE: return "equilibrate";
    case EQ_LOCKED:      return "locked";
    case EQ_RELEASE:     return "release";
    default:             return "idle";
    }
}
