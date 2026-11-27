#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/pressure_sensor.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/fraction_periodic.h"
#include "control/reflux.h"
#include "control/volume_feedback.h"
#include "control/ctrl_scale.h"
#include "esp_timer.h"

#include <math.h>

static float s_total_feed;
static float s_total_prod;
static float s_total_barda;
static float s_feed_ramp;
static float s_run_time;
static bool  s_stable;
static volume_feedback_t s_fb;

static proc_stage_t n_init(void) {
    s_total_feed = s_total_prod = s_total_barda = 0;
    s_feed_ramp = 0;
    s_run_time = 0;
    s_stable = false;
    volume_feedback_begin(&s_fb);
    volume_feedback_set_target(&s_fb, state_get()->target_body_ml,
        state_get()->target_body_ml > 0 ? state_get()->target_body_ml * 1.20f : 0, 120000);
    ctrl_pid_init_ext(18, 0.6f, 2.5f, 96.0f, (float)PID_TIMESTEP_MS, 0);
    if (hw_has_device(HW_DEV_REFLUX)) {
        reflux_set_ratio(3.0f);
        reflux_enable(false);
    }
    events_log(EVENT_INFO, "NBK: init");
    return ST_WARMUP;
}

static proc_stage_t n_update(float dt) {
    float cube = hw_temp_read(HW_TEMP_CUBE);
    float col = hw_temp_read(HW_TEMP_COLUMN);
    process_state_t *st = state_get();
    if (!isfinite(cube)) {
        ctrl_heater_force_off();
        ctrl_dose_stop();
        ctrl_valves_close_all();
        if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
        st->stage = ST_ERROR;
        st->cube_temp = 0.0f;
        st->column_temp = isfinite(col) ? col : 0.0f;
        st->volume_guard_active = false;
        st->volume_no_flow = false;
        st->volume_overflow = false;
        st->eq_lock_active = false;
        st->eq_lock_phase = 0;
        st->flood_active = false;
        scale_update_state(st, false, 0.0f);
        return ST_ERROR;
    }
    st->cube_temp = cube;
    st->column_temp = isfinite(col) ? col : 0.0f;

    if (fsel_mode_tick(st, 50.0f, 60.0f, 75.0f))
        return st->stage;

    bool has_pressure = hw_has_device(HW_DEV_PRESSURE);
    float p_bar = has_pressure ? column_pressure_bar() : 0.0f;
    st->pressure_bar = p_bar;

    s_run_time += dt;

    // Прогрев: подача закрыта, мощность ограничена.
    if (cube < 90.0f) {
        ctrl_heater_set_percent(80.0f);
        if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
        st->stage = ST_WARMUP;
        return ST_WARMUP;
    }

    // Стабилизация: ждём колонну, только потом подаём брагу.
    if (!s_stable) {
        ctrl_heater_set_percent(60.0f);
        bool col_ok = isfinite(col) && col > 70.0f;
        if (col_ok || s_run_time > 600.0f) {
            s_stable = true;
            if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(true);
            events_log(EVENT_STAGE_CHANGE, "NBK: stabilized");
        } else {
            st->stage = ST_STABILIZE;
            return ST_STABILIZE;
        }
    }

    float target = 96.0f;
    ctrl_pid_set_setpoint(target);
    ctrl_pid_compute_and_apply(cube);

    // Давление в колонне — ограничитель подачи, а не авария.
    float limit = 1.0f;
    if (has_pressure && p_bar > 0.2f) limit = 0.4f;
    if (has_pressure && p_bar > 0.35f) limit = 0.15f;

    float target_rate = st->body_rate_ml_min * limit;
    if (!isfinite(target_rate) || target_rate < 0.0f) target_rate = 0.0f;

    // Плавный выход на режим за 60 секунд.
    float ramp_step = (st->body_rate_ml_min > 0.0f)
                          ? st->body_rate_ml_min / 60.0f : 0.0f;
    s_feed_ramp += ramp_step * dt;
    if (s_feed_ramp > target_rate) s_feed_ramp = target_rate;

    if (hw_temp_rising(HW_TEMP_CUBE))
        s_feed_ramp *= PREDEC_TREND_CUT;

    ctrl_dose_process(s_feed_ramp, dt, VALVE_BODY);

    float feed_lph = s_feed_ramp / 1000.0f * 60.0f;
    s_total_feed += feed_lph * dt / 3600.0f;
    float prod_lph = feed_lph * 0.1f;
    s_total_prod += prod_lph * dt / 3600.0f;
    s_total_barda = s_total_feed - s_total_prod;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    volume_feedback_tick(&s_fb, s_feed_ramp > 0.01f, now_ms);
    st->volume_guard_active = true;
    st->volume_no_flow = volume_feedback_no_flow(&s_fb);
    st->volume_overflow = volume_feedback_overflow(&s_fb);
    if (volume_feedback_overflow(&s_fb)) {
        events_log(EVENT_WARNING, "NBK: overflow guard");
        ctrl_dose_stop();
    }
    if (volume_feedback_no_flow(&s_fb)) {
        events_log(EVENT_WARNING, "NBK: no-flow detected");
    }
    st->flood_active = false;
    st->eq_lock_active = false;
    st->eq_lock_phase = 0;

    st->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void n_stop(void) {
    ctrl_heater_force_off();
    ctrl_dose_stop();
    ctrl_valves_close_all();
    hal_relay_all_off();
    if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
}

const process_mode_t MODE_NBK = {
    .name = "НБК", .id = "nbk",
    .init = n_init, .update = n_update, .stop = n_stop,
};
