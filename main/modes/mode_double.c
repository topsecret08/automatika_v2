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
#include "control/ctrl_scale.h"
#include "control/volume_feedback.h"
#include "esp_timer.h"

#include <math.h>

static int s_pass = 1;
static volume_feedback_t s_fb;

static proc_stage_t db_init(void) {
    s_pass = 1;
    volume_feedback_begin(&s_fb);
    volume_feedback_set_target(&s_fb, state_get()->target_body_ml,
        state_get()->target_body_ml > 0 ? state_get()->target_body_ml * 1.20f : 0, 120000);
    return ST_PRECHECK;
}

static proc_stage_t db_update(float dt) {
    (void)dt;
    float cube = hw_temp_read(HW_TEMP_CUBE);
    if (!isfinite(cube)) {
        ctrl_heater_force_off();
        ctrl_valves_close_all();
        process_state_t *st_err = state_get();
        st_err->stage = ST_ERROR;
        st_err->volume_guard_active = false;
        st_err->volume_no_flow = false;
        st_err->volume_overflow = false;
        st_err->eq_lock_active = false;
        st_err->eq_lock_phase = 0;
        st_err->flood_active = false;
        scale_update_state(st_err, false, 0.0f);
        return ST_ERROR;
    }
    process_state_t *st = state_get();
    st->cube_temp = cube;

    if (fsel_mode_tick(st, 60.0f, 65.0f, 75.0f)) {
        float measured = volume_feedback_ml(&s_fb);
        scale_update_state(st, true, measured >= 0.0f ? measured : 0.0f);
        return st->stage;
    }

    float p_hpa = baro_get_pressure_hpa();
    float stop1 = baro_correct_boiling_point(98.0f, p_hpa);
    float stop2 = baro_correct_boiling_point(92.0f, p_hpa);

    ctrl_heater_set_percent(70.0f);
    bool flow_expected = hw_has_device(HW_DEV_VALVES);
    if (flow_expected) valve_set_pct(VALVE_BODY, s_pass == 1 ? 40.0f : 20.0f);

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    volume_feedback_tick(&s_fb, flow_expected, now_ms);
    st->volume_guard_active = true;
    st->volume_no_flow = volume_feedback_no_flow(&s_fb);
    st->volume_overflow = volume_feedback_overflow(&s_fb);
    if (volume_feedback_overflow(&s_fb)) {
        events_log(EVENT_WARNING, "Double: overflow guard");
        ctrl_heater_force_off();
        ctrl_valves_close_all();
        st->is_running = false;
        st->stage = ST_DONE;
        scale_update_state(st, true, 0.0f);
        return ST_DONE;
    }
    if (volume_feedback_no_flow(&s_fb)) {
        events_log(EVENT_WARNING, "Double: no-flow detected");
        ctrl_valves_close_all();
    }

    if (cube > stop1 && s_pass == 1) {
        s_pass = 2;
        events_log(EVENT_STAGE_CHANGE, "Double: pass 2");
    } else if (cube > stop2 && s_pass == 2) {
        ctrl_heater_force_off();
        ctrl_valves_close_all();
        st->is_running = false;
        st->stage = ST_DONE;
        {
            float measured = volume_feedback_ml(&s_fb);
            scale_update_state(st, true, measured >= 0.0f ? measured : 0.0f);
        }
        return ST_DONE;
    }
    st->stage = ST_RUNNING;
    {
        float measured = volume_feedback_ml(&s_fb);
        scale_update_state(st, true, measured >= 0.0f ? measured : 0.0f);
    }
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
