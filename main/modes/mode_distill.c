#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/baro.h"
#include "hal/pressure_sensor.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_forecast.h"
#include "control/fraction_logic.h"
#include "control/fraction_periodic.h"
#include "control/volume_feedback.h"
#include "control/ctrl_scale.h"
#include "core/hw_profile.h"
#include "core/loop_verify.h"
#include "core/eq_lock.h"
#include "core/stab_detect.h"
#include "hal/hal_actuator_readback.h"
#include "esp_timer.h"
#include "control/reflux.h"
#include "web/telegram_notify.h"
#include "config.h"
#include <math.h>

static proc_stage_t s_stage = ST_IDLE;
static float s_stage_t = 0;
static int s_predec_logged = -1;
static fraction_t s_frac = FRAC_HEADS;
static bool s_reflux_on = false;
static bool s_stab_before_body = false;
static volume_feedback_t s_heads_fb;
static volume_feedback_t s_body_fb;
static volume_feedback_t s_tails_fb;
static eq_lock_t s_eq;
static stab_detector_t s_stab;

#define PREHEAT_POWER   100.0f
#define BODY_POWER      60.0f
#define TAIL_POWER      75.0f
#define TAIL_START_T    94.0f

static proc_stage_t d_init(void) {
    s_stage = ST_PRECHECK;
    s_stage_t = 0;
    s_predec_logged = -1;
    s_frac = FRAC_HEADS;
    s_reflux_on = false;
    s_stab_before_body = false;
    eq_lock_init(&s_eq);
    stab_init(&s_stab, 0.15f, 0.02f, 45.0f, 1800.0f, esp_timer_get_time());
    volume_feedback_begin(&s_heads_fb);
    volume_feedback_begin(&s_body_fb);
    volume_feedback_begin(&s_tails_fb);
    volume_feedback_set_target(&s_heads_fb, state_get()->target_heads_ml, state_get()->target_heads_ml > 0 ? state_get()->target_heads_ml * 1.20f : 0, 120000);
    volume_feedback_set_target(&s_body_fb,  state_get()->target_body_ml,  state_get()->target_body_ml  > 0 ? state_get()->target_body_ml  * 1.20f : 0, 120000);
    volume_feedback_set_target(&s_tails_fb, state_get()->target_tails_ml, state_get()->target_tails_ml > 0 ? state_get()->target_tails_ml * 1.20f : 0, 120000);

    loop_verify_init();

    fraction_reset();
    reflux_set_ratio(REFLUX_RATIO_DEFAULT);
    reflux_enable(false);

    events_log(EVENT_INFO, "Distill: init");
    return s_stage;
}

static float predec_factor(float cube) {
    if (!flags_get_by_key("predecrement")) return 1.0f;

    process_state_t *st = state_get();
    float dist = st->predec_distance;
    float factor = 1.0f;

    /* упреждающее снижение отбора вблизи границы хвостов */
    float rate = ctrl_forecast_rate();
    if ((cube - TAIL_START_T) > -dist && rate > 0.05f) {
        factor *= (1.0f - st->predec_step);
        if (s_predec_logged != (int)st->predec_count) {
            st->predec_count++;
            s_predec_logged = (int)st->predec_count;
        }
    }

    /* трендовый срез: температура растёт быстрее нормы */
    if (hal_1wire_rising(0)) factor *= PREDEC_TREND_CUT;

    return factor;
}

static void fraction_apply(fraction_t f) {
    if (f == s_frac) return;
    s_frac = f;

    events_log(EVENT_STAGE_CHANGE, "Fraction -> %s", fraction_name(f));
    telegram_notify_alert("ФРАКЦИЯ", fraction_name(f));

    // Флегма нужна на теле и хвостах; на головах работает прямой отбор.
    if (f == FRAC_BODY || f == FRAC_TAILS) {
        if (!s_reflux_on) {
            s_reflux_on = true;
            if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(true);
        }
    } else {
        if (s_reflux_on) {
            s_reflux_on = false;
            if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
        }
    }
}

static proc_stage_t d_update(float dt) {
    s_stage_t += dt;

    float cube = hw_temp_read(HW_TEMP_CUBE);
    float col  = hw_temp_read(HW_TEMP_COLUMN);
    process_state_t *st = state_get();
    if (!isfinite(cube)) {
        ctrl_heater_force_off();
        ctrl_dose_stop();
        ctrl_valves_close_all();
        if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
        s_stage = ST_ERROR;
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

    float pres_hpa = baro_get_pressure_hpa();
    float cube_corr = baro_correct_boiling_point(cube, pres_hpa);
    bool has_cooling = hw_has_device(HW_DEV_COOLING_PUMP);
    bool has_reflux = hw_has_device(HW_DEV_REFLUX);

    ctrl_forecast_update(cube, dt);

    st->cube_temp = cube;
    st->column_temp = isfinite(col) ? col : 0.0f;
    st->pressure_bar = hw_has_device(HW_DEV_PRESSURE)
                           ? column_pressure_bar() : 0.0f;

    // Периодический отбор — альтернативная логика отбора. Когда она включена,
    // штатный fraction_update/valve/dose не должен работать: иначе два
    // алгоритма одновременно управляют одним клапаном. Нагрев, reflux и
    // охлаждение берёт на себя fsel_mode_tick().
    if (fsel_mode_tick(st, 40.0f, BODY_POWER, TAIL_POWER)) {
        float total_ml = st->heads_collected_ml + st->body_collected_ml + st->tails_collected_ml;
        scale_update_state(st, true, total_ml);
        return st->stage;
    }

    float rate = st->body_rate_ml_min;
    float total_ml = st->heads_collected_ml + st->body_collected_ml +
                     st->tails_collected_ml;

    // Логика фракций работает от скорректированной температуры и объёма.
    fraction_t f = fraction_update(cube_corr, total_ml);
    fraction_apply(f);

    switch (s_stage) {
    case ST_PRECHECK:
        if (hw_has_device(HW_DEV_COOLING_PUMP)) {
            hal_relay_set(RELAY_COOLING_PUMP, true);
            hal_relay_set(RELAY_WATER_VALVE, true);
        }
        if (!ctrl_safety_ok()) { s_stage = ST_ERROR; break; }
        s_stage = ST_HEATUP;
        break;

    case ST_HEATUP:
        ctrl_heater_set_percent(PREHEAT_POWER);
        if (cube > 70.0f) { s_stage = ST_STABILIZE; s_stage_t = 0; }
        break;

    case ST_STABILIZE: {
        int64_t now = esp_timer_get_time();
        if (eq_lock_phase(&s_eq) == EQ_IDLE) {
            eq_lock_engage(&s_eq, ctrl_heater_get_percent(), 1800, now);
        }
        eq_lock_input_t eq_in = {
            .cube_c = cube, .column_c = isfinite(col) ? col : cube,
            .body_valve_closed = true,
            .safety_ok = ctrl_safety_ok() && !estop_actions_blocked(),
        };
        eq_lock_tick(&s_eq, &eq_in, dt, now);
        eq_phase_t eq_ph = eq_lock_phase(&s_eq);
        st->eq_lock_active = (eq_ph != EQ_IDLE);
        st->eq_lock_phase = (uint8_t)eq_ph;
        if (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE || eq_ph == EQ_LOCKED) {
            ctrl_heater_set_percent(s_eq.power_current);
        } else {
            ctrl_heater_set_percent(50.0f);
        }
        bool converged = stab_tick(&s_stab, isfinite(col) ? col : cube, st->target_column_t > 0 ? st->target_column_t : 78.0f, now);
        bool ready = (eq_ph == EQ_LOCKED) || converged || s_stage_t > 900.0f;
        if (ready) {
            if (eq_lock_phase(&s_eq) != EQ_IDLE) eq_lock_release(&s_eq);
            st->eq_lock_active = false;
            st->eq_lock_phase = (uint8_t)EQ_IDLE;
            if (s_stab_before_body) {
                s_stab_before_body = false;
                s_stage = ST_BODY;
                volume_feedback_begin(&s_body_fb);
                events_log(EVENT_STAGE_CHANGE, "-> BODY (restabilized)");
            } else {
                s_stage = ST_HEADS;
                volume_feedback_begin(&s_heads_fb);
                events_log(EVENT_STAGE_CHANGE, "-> HEADS");
            }
        }
        break;
    }

    case ST_HEADS: {
        int64_t now = esp_timer_get_time();
        if (eq_lock_phase(&s_eq) == EQ_IDLE) eq_lock_engage(&s_eq, ctrl_heater_get_percent(), 1800, now);
        eq_lock_input_t eq_in = {
            .cube_c = cube, .column_c = isfinite(col) ? col : cube,
            .body_valve_closed = true,
            .safety_ok = ctrl_safety_ok() && !estop_actions_blocked(),
        };
        eq_lock_tick(&s_eq, &eq_in, dt, now);
        eq_phase_t eq_ph = eq_lock_phase(&s_eq);
        st->eq_lock_active = (eq_ph != EQ_IDLE);
        st->eq_lock_phase = (uint8_t)eq_ph;
        bool eq_guard_pause = (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE);
        if (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE || eq_ph == EQ_LOCKED)
            ctrl_heater_set_percent(s_eq.power_current);
        else
            ctrl_heater_set_percent(40.0f);
        float heads_pct = eq_guard_pause ? 0.0f : 20.0f;
        valve_set_pct(VALVE_HEADS, heads_pct);
        {
            float flow = control_config_get_flow_ml_min();
            float est = flow * (heads_pct / 100.0f) * dt / 60.0f;
            float measured = volume_feedback_ml(&s_heads_fb);
            if (measured >= 0.0f) st->heads_collected_ml = measured;
            else st->heads_collected_ml += est;
            volume_feedback_tick(&s_heads_fb, heads_pct > 0.0f, (uint32_t)(now / 1000));
            st->volume_guard_active = true;
            st->volume_no_flow = volume_feedback_no_flow(&s_heads_fb);
            st->volume_overflow = volume_feedback_overflow(&s_heads_fb);
            if (volume_feedback_overflow(&s_heads_fb)) {
                valve_set_pct(VALVE_HEADS, 0.0f);
                events_log(EVENT_WARNING, "Distill: heads overflow guard");
            }
            if (volume_feedback_no_flow(&s_heads_fb)) {
                valve_set_pct(VALVE_HEADS, 0.0f);
                events_log(EVENT_WARNING, "Distill: heads no-flow");
            }
        }
        if (f >= FRAC_BODY || st->heads_collected_ml > st->target_heads_ml || volume_feedback_overflow(&s_heads_fb)) {
            if (eq_lock_phase(&s_eq) != EQ_IDLE) eq_lock_release(&s_eq);
            s_stab_before_body = true;
            s_stage = ST_STABILIZE;
            eq_lock_init(&s_eq);
            stab_init(&s_stab, 0.15f, 0.02f, 30.0f, 1200.0f, now);
            events_log(EVENT_STAGE_CHANGE, "-> STABILIZE (restab before body)");
        }
        break;
    }

    case ST_BODY: {
        int64_t now = esp_timer_get_time();
        float target_col = st->target_column_t > 0.0f ? st->target_column_t : 78.0f;
        if (eq_lock_phase(&s_eq) == EQ_IDLE) eq_lock_engage(&s_eq, ctrl_heater_get_percent(), 3600, now);
        eq_lock_input_t eq_in = {
            .cube_c = cube, .column_c = isfinite(col) ? col : cube,
            .body_valve_closed = false,
            .safety_ok = ctrl_safety_ok() && !estop_actions_blocked(),
        };
        eq_lock_tick(&s_eq, &eq_in, dt, now);
        eq_phase_t eq_ph = eq_lock_phase(&s_eq);
        st->eq_lock_active = (eq_ph != EQ_IDLE);
        st->eq_lock_phase = (uint8_t)eq_ph;
        bool eq_hold = (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE) ||
                       (isfinite(col) && fabsf(target_col - col) > 0.20f);
        if (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE || eq_ph == EQ_LOCKED)
            ctrl_heater_set_percent(s_eq.power_current);
        else
            ctrl_heater_set_percent(BODY_POWER);

        if (flags_get_by_key("auto92") && cube > 85.0f) {
            float factor = 6.04f - 0.06f * cube;
            if (factor < 0.1f) factor = 0.1f;
            rate *= factor;
        }
        rate *= predec_factor(cube);
        if (eq_hold) rate = 0.0f;

        ctrl_dose_process(rate, dt, VALVE_BODY);
        {
            float measured = volume_feedback_ml(&s_body_fb);
            if (measured >= 0.0f) st->body_collected_ml = measured;
            else st->body_collected_ml += rate * dt / 60.0f;
            volume_feedback_tick(&s_body_fb, rate > 0.01f, (uint32_t)(now / 1000));
            st->volume_guard_active = true;
            st->volume_no_flow = volume_feedback_no_flow(&s_body_fb);
            st->volume_overflow = volume_feedback_overflow(&s_body_fb);
            if (volume_feedback_overflow(&s_body_fb)) { ctrl_dose_stop(); events_log(EVENT_WARNING, "Distill: body overflow guard"); }
            if (volume_feedback_no_flow(&s_body_fb)) { ctrl_dose_stop(); events_log(EVENT_WARNING, "Distill: body no-flow"); }
        }

        if (f >= FRAC_TAILS || cube >= TAIL_START_T) {
            if (eq_lock_phase(&s_eq) != EQ_IDLE) eq_lock_release(&s_eq);
            s_stage = ST_TAILS;
            volume_feedback_begin(&s_tails_fb);
            events_log(EVENT_STAGE_CHANGE, "-> TAILS");
        }
        break;
    }

    case ST_TAILS:
        st->eq_lock_active = false;
        st->eq_lock_phase = (uint8_t)EQ_IDLE;
        ctrl_heater_set_percent(TAIL_POWER);
        valve_set_pct(VALVE_TAILS, 30.0f);
        {
            float flow = control_config_get_flow_ml_min();
            float est = flow * 0.30f * dt / 60.0f;
            float measured = volume_feedback_ml(&s_tails_fb);
            if (measured >= 0.0f) st->tails_collected_ml = measured;
            else st->tails_collected_ml += est;
            volume_feedback_tick(&s_tails_fb, true, (uint32_t)(esp_timer_get_time() / 1000));
            st->volume_guard_active = true;
            st->volume_no_flow = volume_feedback_no_flow(&s_tails_fb);
            st->volume_overflow = volume_feedback_overflow(&s_tails_fb);
            if (volume_feedback_overflow(&s_tails_fb)) { valve_set_pct(VALVE_TAILS, 0.0f); events_log(EVENT_WARNING, "Distill: tails overflow guard"); }
            if (volume_feedback_no_flow(&s_tails_fb)) { valve_set_pct(VALVE_TAILS, 0.0f); events_log(EVENT_WARNING, "Distill: tails no-flow"); }
        }

        if (f >= FRAC_DONE || cube > st->target_cube_t) {
            s_stage = ST_COOLDOWN;
            events_log(EVENT_STAGE_CHANGE, "-> COOLDOWN");
        }
        break;

    case ST_COOLDOWN:
        ctrl_heater_force_off();
        ctrl_dose_stop();
        ctrl_valves_close_all();
        if (has_reflux) reflux_enable(false);
        if (cube < 40.0f) s_stage = ST_DONE;
        break;

    case ST_DONE:
        if (has_cooling) {
            hal_relay_set(RELAY_COOLING_PUMP, false);
            hal_relay_set(RELAY_WATER_VALVE, false);
        }
        st->is_running = false;
        break;

    default:
        break;
    }

    st->stage = s_stage;
    st->flood_active = false;
    if (s_stage != ST_STABILIZE && s_stage != ST_HEADS && s_stage != ST_BODY) {
        st->eq_lock_active = false;
        st->eq_lock_phase = (uint8_t)EQ_IDLE;
    }
    if (s_stage != ST_HEADS && s_stage != ST_BODY && s_stage != ST_TAILS) {
        st->volume_guard_active = false;
        st->volume_no_flow = false;
        st->volume_overflow = false;
    }
    {
        float collected = st->heads_collected_ml + st->body_collected_ml + st->tails_collected_ml;
        bool active = (s_stage == ST_HEADS || s_stage == ST_BODY || s_stage == ST_TAILS ||
                       s_stage == ST_COOLDOWN || s_stage == ST_DONE);
        scale_update_state(st, active, collected);
    }

#if !defined(CONFIG_LOOP_VERIFY_ENABLE) || CONFIG_LOOP_VERIFY_ENABLE
    int64_t now = esp_timer_get_time();
    lv_sensor_frame_t sf[2] = {
        { .id = 0, .raw = cube, .filtered = cube,
          .confidence = isfinite(cube) ? 1.0f : 0.0f },
        { .id = 1, .raw = col, .filtered = col,
          .confidence = isfinite(col) ? 1.0f : 0.0f },
    };
    size_t n_s = hw_temp_available(HW_TEMP_COLUMN) ? 2 : 1;
    lv_program_frame_t pf = {
        .mode = (uint8_t)PROCESS_DISTILL,
        .sp = st->target_cube_t,
        .pv = cube,
        .out = ctrl_heater_get_percent(),
    };
    lv_actuator_frame_t af[1] = {
        { .id = ACT_HEATER, .cmd = ctrl_heater_get_percent(),
          .actual = hal_actuator_readback(ACT_HEATER), .latency_us = 0,
          .binary = hal_actuator_is_binary(ACT_HEATER),
          .response_window_sec = 120 },
    };
    lv_result_t lr;
    int64_t sensor_us = hw_temp_last_update_us(HW_TEMP_CUBE);
    if (sensor_us <= 0) sensor_us = now;
    lv_tick(sensor_us, sf, n_s, &pf, af, 1, &lr);
#endif

    return s_stage;
}

static void d_stop(void) {
    ctrl_heater_force_off();
    ctrl_dose_stop();
    ctrl_valves_close_all();
    if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
    hal_relay_all_off();
}

const process_mode_t MODE_DISTILL = {
    .name = "Дистилляция", .id = "distill",
    .init = d_init, .update = d_update, .stop = d_stop,
};
