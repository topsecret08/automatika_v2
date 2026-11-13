#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_forecast.h"
#include "control/fraction_periodic.h"
#include "config.h"
#include <math.h>

static proc_stage_t s_stage = ST_IDLE;
static float s_stab_time = 0;
static float s_head_stable_time = 0;
static float s_last_col = 0;

#define HEADS_END_T       78.5f
#define BODY_END_T        92.0f
#define STAB_TIME         900.0f
#define STAB_TOL          0.1f

static proc_stage_t r_init(void) {
    s_stage = ST_PRECHECK;
    s_stab_time = 0;
    s_head_stable_time = 0;
    s_last_col = 0;
    events_log(EVENT_INFO, "Rectify: init");
    return s_stage;
}

static bool auto_heads_done(float col, float dt) {
    if (fabsf(col - s_last_col) < STAB_TOL) s_head_stable_time += dt;
    else s_head_stable_time = 0;
    s_last_col = col;
    return s_head_stable_time > STAB_TIME;
}

static proc_stage_t r_update(float dt) {
    float cube = hal_1wire_read(0);
    float col  = hal_1wire_read(1);
    ctrl_forecast_update(cube, dt);
    state_get()->cube_temp = cube;
    state_get()->column_temp = col;

    if (fsel_mode_tick(state_get(), 45.0f, 50.0f, 70.0f))
        return state_get()->stage;

    switch (s_stage) {
    case ST_PRECHECK:
        hal_relay_set(RELAY_COOLING_PUMP, true);
        s_stage = ST_HEATUP;
        break;

    case ST_HEATUP:
        ctrl_heater_set_percent(80.0f);
        if (cube > 70.0f) { s_stage = ST_STABILIZE; s_stab_time = 0; }
        break;

    case ST_STABILIZE:
        ctrl_heater_set_percent(50.0f);
        s_stab_time += dt;
        if ((flags_get_by_key("autodetect_heads") &&
             auto_heads_done(col, dt)) || s_stab_time > 3600.0f) {
            s_stage = ST_HEADS;
            events_log(EVENT_STAGE_CHANGE, "→ HEADS");
        }
        break;

    case ST_HEADS:
        ctrl_heater_set_percent(45.0f);
        valve_set_pct(VALVE_HEADS, 5.0f);
        state_get()->heads_collected_ml += 5.0f * dt;
        if (state_get()->heads_collected_ml > state_get()->target_heads_ml) {
            s_stage = ST_BODY;
            events_log(EVENT_STAGE_CHANGE, "→ BODY");
        }
        break;

    case ST_BODY: {
        float rate = state_get()->body_rate_ml_min;
        if (flags_get_by_key("auto92") && cube > 85.0f) {
            float f = 6.04f - 0.06f * cube;
            if (f < 0.1f) f = 0.1f;
            rate *= f;
        }
        if (flags_get_by_key("predecrement")) {
            float dist = state_get()->predec_distance;
            if ((cube - BODY_END_T) > -dist &&
                ctrl_forecast_rate() > 0.05f) {
                rate *= (1.0f - state_get()->predec_step);
                state_get()->predec_count++;
            }
        }
        /* упреждающий анализ: рост температуры → снизить отбор */
        if (hal_1wire_rising(0)) rate *= PREDEC_TREND_CUT;

        ctrl_heater_set_percent(50.0f);
        ctrl_dose_process(rate, dt, VALVE_BODY);
        if (cube >= BODY_END_T) {
            s_stage = ST_TAILS;
            events_log(EVENT_STAGE_CHANGE, "→ TAILS");
        }
        break;
    }

    case ST_TAILS:
        ctrl_heater_set_percent(70.0f);
        valve_set_pct(VALVE_TAILS, 25.0f);
        if (cube >= state_get()->tail_stop_temp) {
            s_stage = ST_COOLDOWN;
            events_log(EVENT_STAGE_CHANGE, "→ COOLDOWN");
        }
        break;

    case ST_COOLDOWN:
        ctrl_heater_set_percent(0);
        ctrl_valves_close_all();
        if (cube < 40.0f) s_stage = ST_DONE;
        break;

    case ST_DONE:
        hal_relay_set(RELAY_COOLING_PUMP, false);
        state_get()->is_running = false;
        break;

    default: break;
    }
    state_get()->stage = s_stage;
    return s_stage;
}

static void r_stop(void) {
    ctrl_heater_set_percent(0);
    ctrl_dose_stop();
    ctrl_valves_close_all();
    hal_relay_all_off();
}

const process_mode_t MODE_RECTIFY = {
    .name = "Ректификация", .id = "rectify",
    .init = r_init, .update = r_update, .stop = r_stop,
};