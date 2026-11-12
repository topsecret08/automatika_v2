#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_forecast.h"
#include "config.h"
#include <math.h>

static proc_stage_t s_stage = ST_IDLE;
static float s_stage_t = 0;

#define PREHEAT_POWER   100.0f
#define BODY_POWER      60.0f
#define TAIL_POWER      75.0f
#define TAIL_START_T    94.0f

static proc_stage_t d_init(void) {
    s_stage = ST_PRECHECK;
    s_stage_t = 0;
    events_log(EVENT_INFO, "Distill: init");
    return s_stage;
}

static proc_stage_t d_update(float dt) {
    s_stage_t += dt;
    float cube = hal_1wire_read(0);
    float col  = hal_1wire_read(1);

    ctrl_forecast_update(cube, dt);
    state_get()->cube_temp = cube;
    state_get()->column_temp = col;

    switch (s_stage) {
    case ST_PRECHECK:
        hal_relay_set(RELAY_COOLING_PUMP, true);
        if (!ctrl_safety_ok()) { s_stage = ST_ERROR; break; }
        s_stage = ST_HEATUP;
        break;

    case ST_HEATUP:
        ctrl_heater_set_percent(PREHEAT_POWER);
        if (cube > 70.0f) { s_stage = ST_STABILIZE; s_stage_t = 0; }
        break;

    case ST_STABILIZE:
        ctrl_heater_set_percent(50.0f);
        if (s_stage_t > 600.0f) {
            s_stage = ST_HEADS;
            events_log(EVENT_STAGE_CHANGE, "→ HEADS");
        }
        break;

    case ST_HEADS:
        ctrl_heater_set_percent(40.0f);
        valve_set_pct(VALVE_HEADS, 20.0f);
        state_get()->heads_collected_ml += 5.0f * dt;
        if (state_get()->heads_collected_ml > state_get()->target_heads_ml) {
            s_stage = ST_BODY;
            events_log(EVENT_STAGE_CHANGE, "→ BODY");
        }
        break;

    case ST_BODY: {
        float pwr = BODY_POWER;
        float rate = state_get()->body_rate_ml_min;
        if (flags_get_by_key("auto92") && cube > 85.0f) {
            float f = 6.04f - 0.06f * cube;
            if (f < 0.1f) f = 0.1f;
            rate *= f;
        }
        if (flags_get_by_key("predecrement")) {
            float dist = state_get()->predec_distance;
            if ((cube - TAIL_START_T) > -dist &&
                ctrl_forecast_rate() > 0.05f) {
                rate *= (1.0f - state_get()->predec_step);
                state_get()->predec_count++;
            }
        }
        ctrl_heater_set_percent(pwr);
        valve_set_pct(VALVE_BODY, rate / 60.0f * 100.0f);
        state_get()->body_collected_ml += rate * dt / 60.0f;
        if (cube >= TAIL_START_T) {
            s_stage = ST_TAILS;
            events_log(EVENT_STAGE_CHANGE, "→ TAILS");
        }
        break;
    }

    case ST_TAILS:
        ctrl_heater_set_percent(TAIL_POWER);
        valve_set_pct(VALVE_TAILS, 30.0f);
        state_get()->tails_collected_ml += 15.0f * dt;
        if (cube > state_get()->target_cube_t) {
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

static void d_stop(void) {
    ctrl_heater_set_percent(0);
    ctrl_valves_close_all();
    hal_relay_all_off();
}

const process_mode_t MODE_DISTILL = {
    .name = "Дистилляция", .id = "distill",
    .init = d_init, .update = d_update, .stop = d_stop,
};