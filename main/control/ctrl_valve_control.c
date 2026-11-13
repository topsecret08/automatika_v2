#include "ctrl_valve_control.h"
#include "ctrl_valves.h"
#include "safety_gate.h"
#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "valve_calibration.h"
#include "esp_timer.h"
#include <math.h>

static float s_pct[3] = {0, 0, 0};
static uint32_t s_open_start_ms[3] = {0, 0, 0};

void valve_control_init(void) {
    valve_config_init();
    ctrl_valves_init();
}

void valve_set_pct(int idx, float pct) {
    (void)safety_gate_apply_valve("valve_core", idx, pct);
}

void valve_set_pct_raw(int idx, float pct) {
    valve_config_t *v = valve_config_get(idx);
    if (!v) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    s_pct[idx] = pct;

    // Мёртвая зона
    float pct_eff = (pct < v->dead_zone_pct) ? 0 : pct;

    // Учёт НО/НЗ
    float effective;
    if (v->norm == VALVE_NORMALLY_CLOSED) {
        effective = pct_eff;
    } else {
        effective = 100.0f - pct_eff;
    }

    if (v->invert) effective = 100.0f - effective;

    // Преобразование в угол или импульс
    if (v->mode == VALVE_MODE_SERVO) {
        float angle = v->angle_min + (effective / 100.0f) *
                      (v->angle_max - v->angle_min);
        ctrl_valve_set_pct(idx, angle / 180.0f * 100.0f);
    } else {
        float pulse_ms = effective / 100.0f * v->pulse_period_ms;
        ctrl_valve_pulse(idx, pulse_ms / v->pulse_period_ms * 100.0f);
    }

    // Обновление состояния
    if (idx == 0) state_get()->valve_heads_pct = pct;
    if (idx == 1) state_get()->valve_body_pct  = pct;
    if (idx == 2) state_get()->valve_tails_pct = pct;

    // Калибровка — учёт открытий
    if (valve_calib_is_active(idx)) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (effective > 1.0f && s_open_start_ms[idx] == 0) {
            s_open_start_ms[idx] = now;
        } else if (effective < 1.0f && s_open_start_ms[idx] > 0) {
            valve_calib_record_open(idx, now - s_open_start_ms[idx]);
            s_open_start_ms[idx] = 0;
        }
    }
}

float valve_get_pct(int idx) {
    return (idx >= 0 && idx < 3) ? s_pct[idx] : 0;
}

float valve_get_effective_pct(int idx) {
    valve_config_t *v = valve_config_get(idx);
    if (!v) return 0;
    return (v->norm == VALVE_NORMALLY_CLOSED) ? s_pct[idx] : (100.0f - s_pct[idx]);
}