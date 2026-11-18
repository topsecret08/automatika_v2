#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/baro.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/fraction_periodic.h"
#include "esp_timer.h"

#include <math.h>

static int s_pass = 1;

static proc_stage_t db_init(void) {
    s_pass = 1;
    return ST_PRECHECK;
}

static proc_stage_t db_update(float dt) {
    (void)dt;
    float cube = hw_temp_read(HW_TEMP_CUBE);
    if (!isfinite(cube)) {
        state_get()->stage = ST_ERROR;
        return ST_ERROR;
    }
    state_get()->cube_temp = cube;

    if (fsel_mode_tick(state_get(), 60.0f, 65.0f, 75.0f))
        return state_get()->stage;

    float p_hpa = baro_get_pressure_hpa();
    float stop1 = baro_correct_boiling_point(98.0f, p_hpa);
    float stop2 = baro_correct_boiling_point(92.0f, p_hpa);

    ctrl_heater_set_percent(70.0f);
    if (hw_has_device(HW_DEV_VALVES))
        valve_set_pct(VALVE_BODY, s_pass == 1 ? 40.0f : 20.0f);

    if (cube > stop1 && s_pass == 1) {
        s_pass = 2;
        events_log(EVENT_STAGE_CHANGE, "Double: pass 2");
    } else if (cube > stop2 && s_pass == 2) {
        ctrl_heater_force_off();
        ctrl_valves_close_all();
        state_get()->is_running = false;
        return ST_DONE;
    }
    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void db_stop(void) {
    ctrl_heater_force_off();
    ctrl_valves_close_all();
}

void gabriel_calculate(float volume_l, float abv,
                       float *t1, float *t2, float *t1_dil, float *yield) {
    float k = 0.30f + (abv / 100.0f) * 0.25f;
    *t1 = volume_l * k;
    *t2 = volume_l - *t1;
    float alc = *t1 * abv / 100.0f;
    *t1_dil = alc / 0.28f;
    *yield = alc * 0.85f / 0.65f;
}

const process_mode_t MODE_DOUBLE = {
    .name = "Двойная", .id = "double",
    .init = db_init, .update = db_update, .stop = db_stop,
};
