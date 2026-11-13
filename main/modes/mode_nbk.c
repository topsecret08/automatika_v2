#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/fraction_periodic.h"

static float s_total_feed = 0;
static float s_total_prod = 0;
static float s_total_barda = 0;

static proc_stage_t n_init(void) {
    s_total_feed = s_total_prod = s_total_barda = 0;
    ctrl_pid_init_ext(18, 0.6f, 2.5f, 96.0f, (float)PID_TIMESTEP_MS, 0);
    return ST_WARMUP;
}

static proc_stage_t n_update(float dt) {
    float cube = hal_1wire_read(0);
    float col  = hal_1wire_read(1);
    state_get()->cube_temp = cube;

    if (fsel_mode_tick(state_get(), 50.0f, 60.0f, 75.0f))
        return state_get()->stage;
    state_get()->column_temp = col;

    if (cube < 90.0f) {
        ctrl_heater_set_percent(80.0f);
        state_get()->stage = ST_WARMUP;
        return ST_WARMUP;
    }

    ctrl_pid_set_setpoint(96.0f);
    ctrl_pid_compute_and_apply(cube);

    float feed_lph = state_get()->body_rate_ml_min / 1000.0f * 60.0f;
    s_total_feed += feed_lph * dt / 3600.0f;
    float prod_lph = feed_lph * 0.1f;
    s_total_prod += prod_lph * dt / 3600.0f;
    s_total_barda = s_total_feed - s_total_prod;

    /* непрерывный отбор через дозатор; при росте температуры — меньше */
    float rate = state_get()->body_rate_ml_min;
    if (hal_1wire_rising(0)) rate *= PREDEC_TREND_CUT;
    ctrl_dose_process(rate, dt, VALVE_BODY);
    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void n_stop(void) {
    ctrl_heater_force_off();
    ctrl_dose_stop();
    ctrl_valves_close_all();
    hal_relay_all_off();
}

const process_mode_t MODE_NBK = {
    .name = "НБК", .id = "nbk",
    .init = n_init, .update = n_update, .stop = n_stop,
};