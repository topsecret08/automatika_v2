#include "loop_verify.h"
#include <math.h>
#include <string.h>

#define LV_LATENCY_BUDGET_US 50000u
#define LV_MISMATCH_TICKS     3
#define LV_MISMATCH_TOL       2.0f
#define LV_NO_RESPONSE_SEC   30.0f
#define LV_CONF_MIN           0.3f

typedef struct {
    uint8_t mismatch_streak;
    float   last_cmd;
    float   last_pv;
    float   no_response_sec;
    bool    used;
} lv_act_state_t;

static lv_act_state_t g_act[LV_MAX_ACTUATORS];
static int64_t g_last_tick_us;

void loop_verify_init(void) {
    memset(g_act, 0, sizeof(g_act));
    g_last_tick_us = 0;
}

static void verify_sensors(const lv_sensor_frame_t *s, size_t n,
                           lv_result_t *r) {
    r->min_confidence = 1.0f;
    if (!s) return;
    for (size_t i = 0; i < n; i++) {
        r->sensor_flags |= s[i].flags;
        if (isfinite(s[i].confidence) && s[i].confidence < r->min_confidence)
            r->min_confidence = s[i].confidence;
        if (!isfinite(s[i].filtered) ||
            (isfinite(s[i].confidence) && s[i].confidence < LV_CONF_MIN))
            r->sensor_flags |= LV_FLAG_SENSOR_PLAUSIBILITY;
    }
}

static void verify_program(const lv_program_frame_t *p, lv_result_t *r) {
    if (!p) return;
    r->program_flags = p->flags;
    if (!isfinite(p->out) || p->out < 0.0f || p->out > 100.0f)
        r->program_flags |= LV_FLAG_PROG_PID_SAT;
    if (!isfinite(p->sp) || !isfinite(p->pv))
        r->program_flags |= LV_FLAG_PROG_SP_SANITY;
}

static void verify_actuators(const lv_actuator_frame_t *a, size_t n,
                             const lv_program_frame_t *p, lv_result_t *r,
                             float dt) {
    if (!a || !p) return;
    for (size_t i = 0; i < n; i++) {
        if (a[i].id >= LV_MAX_ACTUATORS) {
            r->actuator_flags |= LV_FLAG_ACT_NO_FEEDBACK;
            continue;
        }
        lv_act_state_t *st = &g_act[a[i].id];
        if (!st->used) {
            st->used = true;
            st->last_cmd = a[i].cmd;
            st->last_pv = p->pv;
        }

        r->actuator_flags |= a[i].flags;

        if (!isfinite(a[i].actual)) {
            r->actuator_flags |= LV_FLAG_ACT_NO_FEEDBACK;
            st->last_cmd = a[i].cmd;
            st->last_pv = p->pv;
            continue;
        }

        float diff = fabsf(a[i].cmd - a[i].actual);
        if (diff > LV_MISMATCH_TOL) st->mismatch_streak++;
        else st->mismatch_streak = 0;
        if (st->mismatch_streak >= LV_MISMATCH_TICKS)
            r->actuator_flags |= LV_FLAG_ACT_MISMATCH;

        if (a[i].latency_us > LV_LATENCY_BUDGET_US)
            r->actuator_flags |= LV_FLAG_LATENCY_OVER;

        bool cmd_changed = fabsf(a[i].cmd - st->last_cmd) > 1.0f;
        bool pv_moved = fabsf(p->pv - st->last_pv) > 0.1f;
        if (cmd_changed) {
            st->no_response_sec = dt;
            st->last_cmd = a[i].cmd;
        } else if (pv_moved) {
            st->no_response_sec = 0.0f;
        } else if (st->no_response_sec > 0.0f) {
            st->no_response_sec += dt;
        }
        if (st->no_response_sec > LV_NO_RESPONSE_SEC)
            r->actuator_flags |= LV_FLAG_NO_RESPONSE;

        st->last_cmd = a[i].cmd;
        st->last_pv = p->pv;
    }
}

static uint32_t lv_aggregate(const lv_result_t *r) {
    if (r->actuator_flags & (LV_FLAG_ACT_MISMATCH | LV_FLAG_NO_RESPONSE))
        return 4u;  // CRITICAL
    if (r->min_confidence < LV_CONF_MIN)
        return 4u;
    if (r->actuator_flags & LV_FLAG_LATENCY_OVER)
        return 2u;  // DEGRADE
    if (r->sensor_flags | r->program_flags | r->actuator_flags)
        return 1u;  // WARN
    return 0u;
}

void lv_tick_at(int64_t now_us, int64_t sensor_us,
                const lv_sensor_frame_t *s, size_t n_s,
                const lv_program_frame_t *p,
                const lv_actuator_frame_t *a, size_t n_a,
                lv_result_t *out) {
    if (!out) return;
    float dt = g_last_tick_us ? (float)(now_us - g_last_tick_us) / 1e6f : 0.0f;
    if (dt < 0.0f) dt = 0.0f;
    g_last_tick_us = now_us;

    memset(out, 0, sizeof(*out));
    out->latency_e2e_us = (sensor_us > 0 && now_us >= sensor_us)
                              ? (uint32_t)(now_us - sensor_us) : 0u;

    verify_sensors(s, n_s, out);
    verify_program(p, out);
    verify_actuators(a, n_a, p, out, dt);

    if (out->latency_e2e_us > LV_LATENCY_BUDGET_US)
        out->actuator_flags |= LV_FLAG_LATENCY_OVER;

    out->aggregate = lv_aggregate(out);
}
