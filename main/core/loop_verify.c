#include "loop_verify.h"
#include <math.h>
#include <string.h>

#define LV_MISMATCH_TICKS     3
#define LV_MISMATCH_TOL       2.0f
#define LV_CONF_MIN           0.3f
#define LV_MAX_DT_SEC         5.0f
#define LV_STUCK_EPS          0.01f
#define LV_CROSSCHECK_TOL     2.0f

typedef struct {
    uint8_t mismatch_streak;
    float   last_cmd;
    float   last_pv;
    float   no_response_sec;
    bool    used;
} lv_act_state_t;

typedef struct {
    float    last_value;
    uint16_t flat_ticks;
    bool     used;
} lv_sensor_state_t;

static lv_act_state_t g_act[LV_MAX_ACTUATORS];
static lv_sensor_state_t g_sensor[LV_MAX_SENSORS];
static int64_t g_last_tick_us;
static lv_result_t g_last_result;
static bool g_have_last;

void loop_verify_init(void) {
    memset(g_act, 0, sizeof(g_act));
    memset(g_sensor, 0, sizeof(g_sensor));
    memset(&g_last_result, 0, sizeof(g_last_result));
    g_last_tick_us = 0;
    g_have_last = false;
}

bool loop_verify_last_result(lv_result_t *out) {
    if (!out || !g_have_last) return false;
    *out = g_last_result;
    return true;
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

        if (s[i].id >= LV_MAX_SENSORS || !isfinite(s[i].filtered)) continue;

        lv_sensor_state_t *st = &g_sensor[s[i].id];
        if (!st->used) {
            st->used = true;
            st->last_value = s[i].filtered;
            st->flat_ticks = 0;
        } else if (fabsf(s[i].filtered - st->last_value) <= LV_STUCK_EPS) {
            if (st->flat_ticks < UINT16_MAX) st->flat_ticks++;
            if (st->flat_ticks >= LV_STUCK_TICKS)
                r->sensor_flags |= LV_FLAG_SENSOR_STUCK;
        } else {
            st->flat_ticks = 0;
            st->last_value = s[i].filtered;
        }
    }

    // Дубликаты одного id — это независимые каналы одного узла.
    // Расхождение между ними — cross-check.
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(s[i].filtered)) continue;
        for (size_t k = i + 1; k < n; k++) {
            if (s[i].id != s[k].id || !isfinite(s[k].filtered)) continue;
            if (fabsf(s[i].filtered - s[k].filtered) > LV_CROSSCHECK_TOL)
                r->sensor_flags |= LV_FLAG_SENSOR_CROSSCHECK;
        }
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

        // Бинарный канал (реле/контактор): команда >0 ожидает 100 %,
        // иначе 0 %. Сравнивать 50 % команды с 100 % отклика нельзя.
        float expected = a[i].binary ? (a[i].cmd > 0.5f ? 100.0f : 0.0f)
                                     : a[i].cmd;
        float diff = fabsf(expected - a[i].actual);
        if (diff > LV_MISMATCH_TOL) st->mismatch_streak++;
        else st->mismatch_streak = 0;
        if (st->mismatch_streak >= LV_MISMATCH_TICKS)
            r->actuator_flags |= LV_FLAG_ACT_MISMATCH;

        if (a[i].latency_us > LV_LATENCY_BUDGET_US)
            r->actuator_flags |= LV_FLAG_LATENCY_OVER;

        // NO_RESPONSE имеет смысл только для активной команды нагрева.
        uint32_t window = a[i].response_window_sec
                              ? a[i].response_window_sec
                              : LV_DEFAULT_RESPONSE_SEC;
        bool cmd_changed = fabsf(a[i].cmd - st->last_cmd) > 1.0f;
        bool pv_moved = fabsf(p->pv - st->last_pv) > 0.1f;
        if (a[i].cmd <= 1.0f || pv_moved) {
            st->no_response_sec = 0.0f;
        } else if (cmd_changed) {
            st->no_response_sec = dt;
        } else if (st->no_response_sec > 0.0f) {
            st->no_response_sec += dt;
        }
        if (st->no_response_sec > (float)window)
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

    // Долгий разрыв (пауза, простой между прогонами) — начало новой сессии.
    // Иначе накопленное окно NO_RESPONSE ложно срабатывает на первом такте.
    if (dt > LV_MAX_DT_SEC) {
        dt = 0.0f;
        for (size_t i = 0; i < LV_MAX_ACTUATORS; i++)
            g_act[i].no_response_sec = 0.0f;
    }
    g_last_tick_us = now_us;

    memset(out, 0, sizeof(*out));
    out->latency_e2e_us = (sensor_us > 0 && now_us >= sensor_us)
                              ? (uint32_t)(now_us - sensor_us)
                              : 0u;

    verify_sensors(s, n_s, out);
    verify_program(p, out);
    verify_actuators(a, n_a, p, out, dt);

    if (out->latency_e2e_us > LV_LATENCY_BUDGET_US)
        out->actuator_flags |= LV_FLAG_LATENCY_OVER;

    out->aggregate = lv_aggregate(out);

    g_last_result = *out;
    g_have_last = true;
}

bool lv_escalation_changed(uint32_t aggregate, uint32_t detail,
                           uint32_t *last_aggregate,
                           uint32_t *last_detail) {
    if (!last_aggregate || !last_detail) return true;
    bool changed = (aggregate != *last_aggregate) ||
                   (detail != *last_detail);
    *last_aggregate = aggregate;
    *last_detail = detail;
    return changed;
}
