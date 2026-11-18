#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/hal_adc.h"
#include "level/level_sensor.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/fraction_periodic.h"

#include <math.h>

static bool s_low_level_latched;

static proc_stage_t p_init(void) {
    s_low_level_latched = false;
    ctrl_pid_init_ext(20, 0.8f, 3, state_get()->target_cube_t,
                      (float)PID_TIMESTEP_MS, 0);
    return ST_PRECHECK;
}

static proc_stage_t p_update(float dt) {
    (void)dt;
    process_state_t *st = state_get();
    float jacket = hw_temp_read(HW_TEMP_JACKET);
    float pres = hw_has_device(HW_DEV_PRESSURE) ? hal_adc_pressure_bar() : 0.0f;
    st->jacket_temp = jacket;
    st->pressure_bar = pres;

    if (!isfinite(jacket)) {
        st->stage = ST_ERROR;
        return ST_ERROR;
    }

    if (fsel_mode_tick(st, 50.0f, 60.0f, 75.0f))
        return st->stage;

    // Уровень в рубашке/баке, если датчик отмечен в профиле.
    if (hw_has_device(HW_DEV_LEVEL) && level_is_available() &&
        level_is_too_low()) {
        ctrl_heater_force_off();
        if (!s_low_level_latched) {
            s_low_level_latched = true;
            events_log(EVENT_WARNING, "PVK: low level, heating blocked");
        }
        st->stage = ST_ERROR;
        return ST_ERROR;
    }
    s_low_level_latched = false;

    // Предел давления: сначала срезаем мощность, затем снимаем нагрев.
    float scale = 1.0f;
    if (pres > 2.5f) scale = 0.5f;
    if (pres > 3.0f) {
        ctrl_heater_force_off();
        st->stage = ST_ERROR;
        return ST_ERROR;
    }

    float target = st->target_column_t > 0.0f ? st->target_column_t
                                              : st->target_cube_t;
    if (target < 30.0f) target = 60.0f;
    bool water_mode = target < 100.0f;
    if (hw_has_device(HW_DEV_WATER_VALVE)) {
        // Гистерезис вокруг целевой температуры рубашки.
        if (water_mode && jacket < target - 2.0f)
            hal_relay_set(RELAY_WATER_VALVE, true);
        else if (water_mode && jacket > target + 1.0f)
            hal_relay_set(RELAY_WATER_VALVE, false);
        else if (!water_mode)
            hal_relay_set(RELAY_WATER_VALVE, false);
    }

    ctrl_pid_set_setpoint(target);
    ctrl_pid_compute_and_apply_scaled(jacket, scale);

    st->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void p_stop(void) {
    ctrl_heater_force_off();
    hal_relay_set(RELAY_WATER_VALVE, false);
}

const process_mode_t MODE_PVK = {
    .name = "ПВК", .id = "pvk",
    .init = p_init, .update = p_update, .stop = p_stop,
};
