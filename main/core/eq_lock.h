#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EQ_IDLE = 0, EQ_PREPARE, EQ_RAMP_DOWN,
    EQ_EQUILIBRATE, EQ_LOCKED, EQ_RELEASE
} eq_phase_t;

typedef struct {
    eq_phase_t phase;
    int64_t    phase_start_us;
    float      power_start;
    float      power_current;
    float      stable_acc_sec;
    float      last_cube, last_col;
    int64_t    last_sample_us;
    uint32_t   timeout_sec;
    bool       release_pending;
    float      ramp_sec;
    float      min_power;
    float      rate_tol;
    float      stable_sec;
} eq_lock_t;

typedef struct {
    float cube_c;
    float column_c;
    bool  body_valve_closed;
    bool  safety_ok;
} eq_lock_input_t;

void eq_lock_init(eq_lock_t *eq);
bool eq_lock_engage(eq_lock_t *eq, float current_power_pct,
                    uint32_t timeout_sec, int64_t now_us);
bool eq_lock_tick(eq_lock_t *eq, const eq_lock_input_t *in,
                  float dt, int64_t now_us);
bool eq_lock_release(eq_lock_t *eq);
eq_phase_t eq_lock_phase(const eq_lock_t *eq);
const char *eq_lock_phase_name(eq_phase_t phase);
