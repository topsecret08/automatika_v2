#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_valve_control.h"

#include <math.h>

static float s_target;
static float s_hyst;

static proc_stage_t f_init(void) {
    s_target = state_get()->target_cube_t > 0.0f
                   ? state_get()->target_cube_t : 20.0f;
    s_hyst = state_get()->target_column_t > 0.0f &&
                     state_get()->target_column_t < 5.0f
                 ? state_get()->target_column_t : 0.3f;
    return ST_PRECHECK;
}

static proc_stage_t f_update(float dt) {
    (void)dt;
    float t = hw_temp_read(HW_TEMP_TANK);
    if (!isfinite(t)) t = hw_temp_read(HW_TEMP_CUBE);
    if (!isfinite(t)) {
        state_get()->stage = ST_ERROR;
        return ST_ERROR;
    }
    state_get()->cube_temp = t;

    bool cool = hw_has_device(HW_DEV_COOLING_PUMP);
    if (t < s_target - s_hyst) {
        if (cool) hal_relay_set(RELAY_COOLING_PUMP, false);
        ctrl_heater_set_percent(100.0f);
    } else if (t > s_target + s_hyst) {
        ctrl_heater_force_off();
        if (cool) {
            hal_relay_set(RELAY_COOLING_PUMP, true);
            hal_relay_set(RELAY_WATER_VALVE, true);
        }
    } else {
        ctrl_heater_force_off();
        if (cool) {
            hal_relay_set(RELAY_COOLING_PUMP, false);
            hal_relay_set(RELAY_WATER_VALVE, false);
        }
    }

    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void f_stop(void) {
    ctrl_heater_force_off();
    if (hw_has_device(HW_DEV_COOLING_PUMP)) {
        hal_relay_set(RELAY_COOLING_PUMP, false);
        hal_relay_set(RELAY_WATER_VALVE, false);
    }
}

const process_mode_t MODE_FERMENT = {
    .name = "Брожение", .id = "ferment",
    .init = f_init, .update = f_update, .stop = f_stop,
};
