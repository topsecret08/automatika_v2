#include "mode_base.h"
#include "core/state.h"
#include "hal/hal_1wire.h"
#include "hal/hal_adc.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/fraction_periodic.h"
#include "config.h"

static proc_stage_t v_init(void) {
    ctrl_pid_init_ext(15, 0.6f, 2.0f, 40.0f, (float)PID_TIMESTEP_MS, 0);
    return ST_PRECHECK;
}

static proc_stage_t v_update(float dt) {
    (void)dt;
    float cube = hal_1wire_read(0);
    float vac  = hal_adc_vacuum_mbar();
    state_get()->cube_temp = cube;

    if (fsel_mode_tick(state_get(), 50.0f, 60.0f, 70.0f))
        return state_get()->stage;
    state_get()->vacuum_mbar = vac;

    hal_relay_set(RELAY_EXTRA, true);   // вакуум-насос

    float target_t = 40.0f;
    ctrl_pid_set_setpoint(target_t);
    ctrl_pid_compute_and_apply(cube);

    valve_set_pct(VALVE_BODY, 10.0f);
    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void v_stop(void) {
    ctrl_heater_set_percent(0);
    hal_relay_set(RELAY_EXTRA, false);
    ctrl_valves_close_all();
}

const process_mode_t MODE_VACUUM = {
    .name = "Вакуум", .id = "vacuum",
    .init = v_init, .update = v_update, .stop = v_stop,
};