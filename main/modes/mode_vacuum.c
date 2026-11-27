#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/hw_profile.h"
#include "core/antoine.h"
#include "hal/hal_1wire.h"
#include "hal/hal_adc.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/fraction_periodic.h"
#include "control/ctrl_scale.h"
#include "control/volume_feedback.h"
#include "esp_timer.h"

#include <math.h>

static bool s_pump_on;
static bool s_leak_warned;
static float s_leak_time;
static float s_leak_start_mbar;
static volume_feedback_t s_fb;

// Давление насыщения воды при заданной температуре, мбар.
static float pressure_for_temp(float t_c) {
    if (!isfinite(t_c) || t_c < 1.0f) t_c = 40.0f;
    for (int i = 0; i < 2; i++) {
        const antoine_set_t *s = &ANTOINE_WATER[i];
        if (t_c < s->Tmin || t_c > s->Tmax) continue;
        float mmhg = powf(10.0f, s->A - s->B / (s->C + t_c));
        return mmhg * 1.33322f;
    }
    return 73.0f;  // ~40 °C
}

static void pump_set(bool on) {
    if (!hw_has_device(HW_DEV_VACUUM_PUMP)) return;
    hal_relay_set(RELAY_EXTRA, on);
    s_pump_on = on;
}

static proc_stage_t v_init(void) {
    s_pump_on = false;
    s_leak_warned = false;
    s_leak_time = 0;
    s_leak_start_mbar = 0;
    volume_feedback_begin(&s_fb);
    volume_feedback_set_target(&s_fb, state_get()->target_body_ml,
        state_get()->target_body_ml > 0 ? state_get()->target_body_ml * 1.20f : 0, 120000);
    if (hw_has_device(HW_DEV_COOLING_PUMP)) {
        hal_relay_set(RELAY_COOLING_PUMP, true);
        hal_relay_set(RELAY_WATER_VALVE, true);
    }
    pump_set(false);
    ctrl_pid_init_ext(15, 0.6f, 2.0f, 40.0f, (float)PID_TIMESTEP_MS, 0);
    events_log(EVENT_INFO, "Vacuum: init");
    return ST_PRECHECK;
}

static proc_stage_t v_update(float dt) {
    process_state_t *st = state_get();
    float cube = hw_temp_read(HW_TEMP_CUBE);
    float vac = hal_adc_vacuum_mbar();
    st->cube_temp = cube;
    st->vacuum_mbar = vac;

    if (!isfinite(cube) || !isfinite(vac)) {
        ctrl_heater_force_off();
        pump_set(false);
        ctrl_valves_close_all();
        st->stage = ST_ERROR;
        st->volume_guard_active = false;
        st->volume_no_flow = false;
        st->volume_overflow = false;
        st->eq_lock_active = false;
        st->eq_lock_phase = 0;
        st->flood_active = false;
        scale_update_state(st, false, 0.0f);
        return ST_ERROR;
    }

    if (fsel_mode_tick(st, 50.0f, 60.0f, 70.0f)) {
        float measured = volume_feedback_ml(&s_fb);
        scale_update_state(st, true, measured >= 0.0f ? measured : 0.0f);
        return st->stage;
    }

    float target_t = st->target_cube_t > 0.0f ? st->target_cube_t : 40.0f;
    float target_mbar = pressure_for_temp(target_t);
    if (target_mbar < 30.0f) target_mbar = 30.0f;
    if (target_mbar > 1000.0f) target_mbar = 1000.0f;

    // Гистерезис насоса: не дёргать реле на каждом такте.
    if (vac > target_mbar + 15.0f) pump_set(true);
    else if (vac < target_mbar - 10.0f) pump_set(false);

    // Тест утечки: при выключенном насосе давление не должно расти быстро.
    if (!s_pump_on) {
        if (s_leak_start_mbar <= 0.0f) s_leak_start_mbar = vac;
        s_leak_time += dt;
        if (s_leak_time >= 60.0f) {
            float rise = vac - s_leak_start_mbar;
            if (rise > 25.0f && !s_leak_warned) {
                s_leak_warned = true;
                events_log(EVENT_WARNING,
                           "Vacuum: possible leak (%.0f mbar/min)", rise);
            }
            s_leak_time = 0;
            s_leak_start_mbar = vac;
        }
    } else {
        s_leak_time = 0;
        s_leak_start_mbar = vac;
    }

    // Если вакуум ещё не достигнут, не гоним мощность на полную.
    float scale = (vac > target_mbar + 40.0f) ? 0.5f : 1.0f;
    ctrl_pid_set_setpoint(target_t);
    ctrl_pid_compute_and_apply_scaled(cube, scale);

    bool flow_expected = hw_has_device(HW_DEV_VALVES);
    if (flow_expected) valve_set_pct(VALVE_BODY, 10.0f);
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    volume_feedback_tick(&s_fb, flow_expected, now_ms);
    st->volume_guard_active = true;
    st->volume_no_flow = volume_feedback_no_flow(&s_fb);
    st->volume_overflow = volume_feedback_overflow(&s_fb);
    if (volume_feedback_overflow(&s_fb)) {
        events_log(EVENT_WARNING, "Vacuum: overflow guard");
        ctrl_valves_close_all();
    }
    if (volume_feedback_no_flow(&s_fb)) {
        events_log(EVENT_WARNING, "Vacuum: no-flow detected");
        ctrl_valves_close_all();
    }

    st->stage = ST_RUNNING;
    {
        float measured = volume_feedback_ml(&s_fb);
        scale_update_state(st, true, measured >= 0.0f ? measured : 0.0f);
    }
    return ST_RUNNING;
}

static void v_stop(void) {
    ctrl_heater_force_off();
    pump_set(false);
    ctrl_valves_close_all();
    if (hw_has_device(HW_DEV_COOLING_PUMP)) {
        hal_relay_set(RELAY_COOLING_PUMP, false);
        hal_relay_set(RELAY_WATER_VALVE, false);
    }
}

const process_mode_t MODE_VACUUM = {
    .name = "Вакуум", .id = "vacuum",
    .init = v_init, .update = v_update, .stop = v_stop,
};
