#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Многоуровневый адаптивный PID.
//
// L1: фазовый gain scheduling с bumpless-переходом.
// L2: RLS-идентификация модели первого порядка и IMC-пересчёт (ядро 0).
// L3: детектор деградации и откат к верифицированным коэффициентам.
//
// adaptive_pid_step() только считает; применение — через safety_gate_apply()
// при allow_apply=true и verified-флаге.

typedef enum {
    PHASE_HEAT_UP = 0,
    PHASE_SETTLING,
    PHASE_STEADY,
    PHASE_DISTURBANCE,
    PHASE_MAX
} adaptive_phase_t;

typedef struct {
    float kp, ki, kd;

    float phase_kp[PHASE_MAX], phase_ki[PHASE_MAX], phase_kd[PHASE_MAX];
    adaptive_phase_t phase, last_phase;

    // RLS: y[k] = a*y[k-1] + b*u[k-1-d]
    float theta[2];
    float P[2][2];
    float forgetting;
    float y_hist[32], u_hist[32];
    int   hist_idx;
    int   hist_count;
    int   est_delay;
    uint32_t est_counter, est_period;
    float last_dt;

    // Интегратор / bumpless
    float integral, out_min, out_max, last_meas, last_out;
    bool  bumpless_pending;

    // L3
    float osc_index_slow, sse_slow;
    uint32_t bad_cycles;
    float err_hist[60];
    int   err_idx, err_count;

    // Верифицированный откат
    float verified_kp, verified_ki, verified_kd;
    bool  verified_valid;

    // Тень
    float shadow_kp, shadow_ki, shadow_kd, shadow_out;

    // Метрики прогона (последние N шагов)
    float metric_sp[256], metric_y[256];
    int   metric_n;
    bool  metric_active;
    bool  metric_was_applied;

    // Пересчёт L2, ожидающий применения на ядре 1
    float pending_kp, pending_ki, pending_kd;
    bool  pending_gains;
} adaptive_pid_t;

esp_err_t adaptive_pid_init(adaptive_pid_t *p, float kp, float ki, float kd,
                            float out_min, float out_max);
float adaptive_pid_step(adaptive_pid_t *p, float sp, float meas,
                        float dt, bool allow_apply);
void adaptive_pid_freeze_current(adaptive_pid_t *p);

void adaptive_pid_run_reset(adaptive_pid_t *p);
// Возвращает true, если метрики прогона в допуске; сохраняет JSON в verify_runs.
bool adaptive_pid_run_finish(adaptive_pid_t *p, int64_t run_id);

bool adaptive_pid_is_enabled(void);
const char *adaptive_pid_phase_name(adaptive_phase_t phase);

// Глобальный экземпляр, который использует ctrl_pid.
adaptive_pid_t *adaptive_pid_global(void);
