#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/hal_adc.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/fraction_ssvc.h"
#include "config.h"

typedef enum { PVK_WATER, PVK_STEAM, PVK_AUTO } pvk_mode_t;
static pvk_mode_t s_mode = PVK_AUTO;
static proc_stage_t s_stage = ST_IDLE;

static proc_stage_t p_init(void) {
    ctrl_pid_init_ext(20, 0.8f, 3, state_get()->target_cube_t,
                      (float)PID_TIMESTEP_MS, 0);
    s_stage = ST_PRECHECK;
    return s_stage;
}

static proc_stage_t p_update(float dt) {
    (void)dt;
    float jacket = hal_1wire_read(3);
    float cube   = hal_1wire_read(0);
    float pres   = hal_adc_pressure_bar();

    state_get()->jacket_temp = jacket;
    state_get()->cube_temp = cube;

    if (ssvc_mode_tick(state_get(), 50.0f, 60.0f, 75.0f))
        return state_get()->stage;
    state_get()->pressure_bar = pres;

    if (s_mode == PVK_AUTO) {
        if (state_get()->target_cube_t < 100.0f)
            hal_relay_set(RELAY_WATER_VALVE, true);
        else
            hal_relay_set(RELAY_WATER_VALVE, false);
    } else if (s_mode == PVK_WATER) {
        hal_relay_set(RELAY_WATER_VALVE, true);
    } else {
        hal_relay_set(RELAY_WATER_VALVE, false);
    }

    ctrl_pid_set_setpoint(state_get()->target_cube_t);
    float out = ctrl_pid_compute(jacket);
    if (pres > 2.5f) out *= 0.5f;
    ctrl_heater_set_percent(out);

    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void p_stop(void) { ctrl_heater_set_percent(0); }

const process_mode_t MODE_PVK = {
    .name = "ПВК", .id = "pvk",
    .init = p_init, .update = p_update, .stop = p_stop,
};