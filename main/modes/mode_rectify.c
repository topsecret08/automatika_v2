#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/control_config.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/baro.h"
#include "hal/pressure_sensor.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_forecast.h"
#include "control/fraction_periodic.h"
#include "control/volume_feedback.h"
#include "control/alcohol_balance.h"
#include "control/flood_detector.h"
#include "control/ctrl_scale.h"
#include "control/reflux.h"
#include "core/eq_lock.h"
#include "core/stab_detect.h"
#include "control/ctrl_safety.h"
#include "core/estop.h"
#include "esp_timer.h"
#include "config.h"
#include <math.h>

static proc_stage_t s_stage = ST_IDLE;
static float s_stab_time = 0;
static float s_head_stable_time = 0;
static float s_last_col = 0;
static float s_rise_acc = 0.0f;
static float s_flood_time = 0;
static float s_flood_hold = 0;
static bool  s_flooded = false;
static bool  s_stab_before_body = false;
static stab_detector_t s_stab;
static eq_lock_t s_eq;
static volume_feedback_t s_heads_fb;
static volume_feedback_t s_body_fb;
static volume_feedback_t s_tails_fb;
static flood_detector_t s_flood_det;

#define HEADS_END_T       78.5f
#define BODY_END_T        92.0f
#define STAB_TIME         900.0f
#define STAB_TOL          0.1f
#define FLOOD_PRESSURE    0.20f   // бар над атмосферой
#define FLOOD_DROP_T      1.5f
#define FLOOD_HOLD_SEC    45.0f

static proc_stage_t r_init(void) {
    s_stage = ST_PRECHECK;
    s_stab_time = 0;
    s_head_stable_time = 0;
    s_last_col = 0;
    s_rise_acc = 0.0f;
    s_flood_time = 0;
    s_flood_hold = 0;
    s_flooded = false;
    s_stab_before_body = false;
    stab_init(&s_stab, 0.1f, 0.01f, 60.0f, 3600.0f,
              esp_timer_get_time());
    eq_lock_init(&s_eq);
    flood_detector_init(&s_flood_det);
    volume_feedback_begin(&s_heads_fb);
    volume_feedback_begin(&s_body_fb);
    volume_feedback_begin(&s_tails_fb);
    volume_feedback_set_target(&s_heads_fb, state_get()->target_heads_ml, state_get()->target_heads_ml > 0 ? state_get()->target_heads_ml * 1.20f : 0, 120000);
    volume_feedback_set_target(&s_body_fb,  state_get()->target_body_ml,  state_get()->target_body_ml  > 0 ? state_get()->target_body_ml  * 1.20f : 0, 120000);
    volume_feedback_set_target(&s_tails_fb, state_get()->target_tails_ml, state_get()->target_tails_ml > 0 ? state_get()->target_tails_ml * 1.20f : 0, 120000);
    if (hw_has_device(HW_DEV_REFLUX)) {
        reflux_set_ratio(REFLUX_RATIO_DEFAULT);
        reflux_enable(false);
    }
    events_log(EVENT_INFO, "Rectify: init");
    return s_stage;
}

static bool auto_heads_done(float col, float dt) {
    if (fabsf(col - s_last_col) < STAB_TOL) s_head_stable_time += dt;
    else s_head_stable_time = 0;
    bool stable_long = s_head_stable_time > STAB_TIME;

    // Признаковый авто-детект: после плато температура колонны уверенно растёт,
    // что часто означает переход от голов к телу.
    float slope = hw_temp_slope(HW_TEMP_COLUMN);
    bool rising_sign = isfinite(slope) && slope > 0.03f && col > 77.0f;
    if (rising_sign) s_rise_acc += dt; else if (s_rise_acc > 0.0f) s_rise_acc -= dt * 0.5f;
    if (s_rise_acc < 0.0f) s_rise_acc = 0.0f;
    bool rise_sign = s_rise_acc > 120.0f;

    // Доп. признак: крепость/ABV падает, температура куба растёт.
    float abv = abv_get_current_abv_percent();
    bool abv_sign = isfinite(abv) && abv > 0.0f && abv < 55.0f && hw_temp_rising(HW_TEMP_CUBE);

    s_last_col = col;
    return stable_long || rise_sign || (rising_sign && abv_sign);
}

static float corrected(float t, float p_hpa) {
    return baro_correct_boiling_point(t, p_hpa);
}

// Захлёб: рост давления в колонне или резкое падение температуры царги
// при заметной мощности. Логика не срабатывает, если датчик давления
// не отмечен в профиле.
static bool flood_detected(float col, float dt, float p_bar, bool has_pres) {
    flood_input_t fin = {
        .pressure_hpa = has_pres ? p_bar * 10.0f : NAN, // bar->hPa approx? p_bar here is bar over atm in codebase; keep consistent
        .cube_c = NAN,
        .column_c = col,
        .power_pct = ctrl_heater_get_percent(),
        .dt_s = dt,
    };
    // p_bar in this function is bar over atmosphere (FLOOD_PRESSURE 0.20 bar)
    fin.pressure_hpa = has_pres ? p_bar * 1000.0f : NAN;
    flood_detector_tick(&s_flood_det, &fin);
    bool pressure_high = has_pres && p_bar > FLOOD_PRESSURE;
    bool temp_drop = (s_last_col > 0.0f) &&
                     (s_last_col - col > FLOOD_DROP_T) &&
                     ctrl_heater_get_percent() > 30.0f;
    if (pressure_high || temp_drop || flood_detector_active(&s_flood_det)) {
        s_flood_time += dt;
    } else {
        s_flood_time = 0;
    }
    return s_flood_time > 10.0f;
}

static void flood_enter(float col) {
    if (s_flooded) return;
    s_flooded = true;
    s_flood_hold = FLOOD_HOLD_SEC;
    ctrl_heater_set_percent(20.0f);
    if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
    if (hw_has_device(HW_DEV_VALVES)) ctrl_valves_close_all();
    events_log(EVENT_WARNING, "Rectify: flood detected at %.2f C", col);
}

static void flood_release(void) {
    if (!s_flooded) return;
    s_flooded = false;
    s_flood_time = 0;
    events_log(EVENT_INFO, "Rectify: flood cleared");
}

static proc_stage_t r_update(float dt) {
    float cube = hw_temp_read(HW_TEMP_CUBE);
    float col = hw_temp_read(HW_TEMP_COLUMN);
    if (!isfinite(cube) || !isfinite(col)) {
        ctrl_heater_force_off();
        ctrl_dose_stop();
        ctrl_valves_close_all();
        if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
        s_stage = ST_ERROR;
        process_state_t *st_err = state_get();
        st_err->stage = ST_ERROR;
        st_err->cube_temp = isfinite(cube) ? cube : 0.0f;
        st_err->column_temp = isfinite(col) ? col : 0.0f;
        st_err->volume_guard_active = false;
        st_err->volume_no_flow = false;
        st_err->volume_overflow = false;
        st_err->eq_lock_active = false;
        st_err->eq_lock_phase = 0;
        st_err->flood_active = false;
        scale_update_state(st_err, false, 0.0f);
        return ST_ERROR;
    }

    float p_hpa = baro_get_pressure_hpa();
    bool has_pres = hw_has_device(HW_DEV_PRESSURE);
    float p_bar = has_pres ? column_pressure_bar() : 0.0f;
    bool has_reflux = hw_has_device(HW_DEV_REFLUX);
    bool has_valves = hw_has_device(HW_DEV_VALVES);
    bool has_cooling = hw_has_device(HW_DEV_COOLING_PUMP);

    ctrl_forecast_update(cube, dt);
    process_state_t *st = state_get();
    st->cube_temp = cube;
    st->column_temp = col;
    st->pressure_bar = p_bar;

    if (fsel_mode_tick(st, 45.0f, 50.0f, 70.0f)) {
        float collected = st->heads_collected_ml + st->body_collected_ml + st->tails_collected_ml;
        bool active = (st->stage == ST_HEADS || st->stage == ST_BODY || st->stage == ST_TAILS ||
                       st->stage == ST_COOLDOWN || st->stage == ST_DONE);
        scale_update_state(st, active, collected);
        return st->stage;
    }

    if (flood_detected(col, dt, p_bar, has_pres)) flood_enter(col);
    if (s_flooded) {
        s_flood_hold -= dt;
        st->stage = s_stage;
        if (s_flood_hold <= 0.0f) flood_release();
        s_last_col = col;
        {
            float collected = st->heads_collected_ml + st->body_collected_ml + st->tails_collected_ml;
            bool active = (s_stage == ST_HEADS || s_stage == ST_BODY || s_stage == ST_TAILS ||
                           s_stage == ST_COOLDOWN || s_stage == ST_DONE);
            scale_update_state(st, active, collected);
        }
        return s_stage;
    }

    switch (s_stage) {
    case ST_PRECHECK:
        if (has_cooling) {
            hal_relay_set(RELAY_COOLING_PUMP, true);
            hal_relay_set(RELAY_WATER_VALVE, true);
        }
        s_stage = ST_HEATUP;
        break;

    case ST_HEATUP:
        ctrl_heater_set_percent(80.0f);
        if (cube > 70.0f) {
            s_stage = ST_STABILIZE;
            s_stab_time = 0;
        }
        break;

    case ST_STABILIZE:
        if (has_reflux) {
            reflux_set_ratio(8.0f);
            reflux_enable(true);
        }
        s_stab_time += dt;
        {
            int64_t now = esp_timer_get_time();
            if (eq_lock_phase(&s_eq) == EQ_IDLE) {
                eq_lock_engage(&s_eq, ctrl_heater_get_percent(), 3600, now);
            }
            eq_lock_input_t eq_in = {
                .cube_c = cube,
                .column_c = col,
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

            float target = st->target_column_t > 0.0f
                               ? corrected(st->target_column_t, p_hpa)
                               : corrected(HEADS_END_T, p_hpa);
            bool converged = stab_tick(&s_stab, col, target, now);
            bool eq_locked = (eq_ph == EQ_LOCKED);
            if (eq_locked) eq_lock_release(&s_eq);

            bool ready = converged || eq_locked ||
                         (!s_stab_before_body && flags_get_by_key("autodetect_heads") &&
                          auto_heads_done(col, dt));
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
                    events_log(EVENT_STAGE_CHANGE, "-> HEADS (stab)");
                }
            }
        }
        break;

    case ST_HEADS: {
        // EQ_LOCK guard: фиксация стабильного режима капельного отбора голов.
        int64_t now = esp_timer_get_time();
        if (eq_lock_phase(&s_eq) == EQ_IDLE) {
            eq_lock_engage(&s_eq, ctrl_heater_get_percent(), 3600, now);
        }
        eq_lock_input_t eq_in = {
            .cube_c = cube, .column_c = col,
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
            ctrl_heater_set_percent(45.0f);
        }
        if (has_reflux) { reflux_set_ratio(8.0f); reflux_enable(true); }

        bool eq_guard_pause = (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE);
        float heads_pct = eq_guard_pause ? 0.0f : 5.0f;
        if (has_valves) valve_set_pct(VALVE_HEADS, heads_pct);
        {
            float flow = control_config_get_flow_ml_min();
            float est = flow * (heads_pct / 100.0f) * dt / 60.0f;
            float measured = volume_feedback_ml(&s_heads_fb);
            if (measured >= 0.0f) st->heads_collected_ml = measured;
            else st->heads_collected_ml += est;
            uint32_t now_ms = (uint32_t)(now / 1000);
            volume_feedback_tick(&s_heads_fb, has_valves && heads_pct > 0.0f, now_ms);
            st->volume_guard_active = true;
            st->volume_no_flow = volume_feedback_no_flow(&s_heads_fb);
            st->volume_overflow = volume_feedback_overflow(&s_heads_fb);
            if (volume_feedback_overflow(&s_heads_fb)) {
                events_log(EVENT_WARNING, "Heads overflow guard triggered");
                if (eq_lock_phase(&s_eq) != EQ_IDLE) eq_lock_release(&s_eq);
                s_stab_before_body = true;
                s_stage = ST_STABILIZE;
                eq_lock_init(&s_eq);
                events_log(EVENT_STAGE_CHANGE, "-> STABILIZE (restab before body, overflow)");
                break;
            }
            if (volume_feedback_no_flow(&s_heads_fb)) {
                events_log(EVENT_WARNING, "Heads no-flow detected, closing valve");
                if (has_valves) valve_set_pct(VALVE_HEADS, 0.0f);
            }
        }
        if (st->heads_collected_ml > st->target_heads_ml ||
            col >= corrected(HEADS_END_T, p_hpa)) {
            if (eq_lock_phase(&s_eq) != EQ_IDLE) eq_lock_release(&s_eq);
            // Достабилизация после отбора голов перед возобновлением отбора тела.
            s_stab_before_body = true;
            s_stage = ST_STABILIZE;
            eq_lock_init(&s_eq);
            stab_init(&s_stab, 0.1f, 0.01f, 30.0f, 1800.0f, now);
            events_log(EVENT_STAGE_CHANGE, "-> STABILIZE (restab before body)");
        }
        break;
    }

    case ST_BODY: {
        // EQ_LOCK: жесткий сторож спиртовой полки (гистерезис 0.1-0.2C).
        int64_t now = esp_timer_get_time();
        float target_col = st->target_column_t > 0.0f ? st->target_column_t : HEADS_END_T;
        float shelf_err = fabsf(target_col - col);
        if (eq_lock_phase(&s_eq) == EQ_IDLE) {
            eq_lock_engage(&s_eq, ctrl_heater_get_percent(), 3600, now);
        }
        eq_lock_input_t eq_in = {
            .cube_c = cube, .column_c = col,
            .body_valve_closed = false,
            .safety_ok = ctrl_safety_ok() && !estop_actions_blocked(),
        };
        eq_lock_tick(&s_eq, &eq_in, dt, now);
        eq_phase_t eq_ph = eq_lock_phase(&s_eq);
        st->eq_lock_active = (eq_ph != EQ_IDLE);
        st->eq_lock_phase = (uint8_t)eq_ph;

        bool eq_hold = (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE) || (shelf_err > 0.20f);
        if (eq_ph == EQ_RAMP_DOWN || eq_ph == EQ_EQUILIBRATE || eq_ph == EQ_LOCKED) {
            ctrl_heater_set_percent(s_eq.power_current);
        } else {
            ctrl_heater_set_percent(50.0f);
        }
        if (has_reflux) { reflux_set_ratio(REFLUX_RATIO_DEFAULT); reflux_enable(true); }

        float rate = st->body_rate_ml_min;
        float err = target_col - col;
        float factor = 1.0f + 0.08f * err;
        if (factor < 0.3f) factor = 0.3f;
        if (factor > 1.5f) factor = 1.5f;
        rate *= factor;

        if (flags_get_by_key("auto92") && cube > 85.0f) {
            float f = 6.04f - 0.06f * cube;
            if (f < 0.1f) f = 0.1f;
            rate *= f;
        }
        if (flags_get_by_key("predecrement")) {
            float dist = st->predec_distance;
            if ((cube - BODY_END_T) > -dist && ctrl_forecast_rate() > 0.05f) {
                rate *= (1.0f - st->predec_step);
                st->predec_count++;
            }
        }
        if (hw_temp_rising(HW_TEMP_CUBE)) rate *= PREDEC_TREND_CUT;
        if (eq_hold) rate = 0.0f;

        ctrl_dose_process(rate, dt, VALVE_BODY);
        {
            float measured = volume_feedback_ml(&s_body_fb);
            if (measured >= 0.0f) st->body_collected_ml = measured;
            else st->body_collected_ml += rate * dt / 60.0f;
            uint32_t now_ms = (uint32_t)(now / 1000);
            volume_feedback_tick(&s_body_fb, has_valves && rate > 0.01f, now_ms);
            st->volume_guard_active = true;
            st->volume_no_flow = volume_feedback_no_flow(&s_body_fb);
            st->volume_overflow = volume_feedback_overflow(&s_body_fb);
            if (volume_feedback_overflow(&s_body_fb)) {
                events_log(EVENT_WARNING, "Body overflow guard triggered");
                ctrl_dose_stop();
                if (has_valves) ctrl_valves_close_all();
            }
            if (volume_feedback_no_flow(&s_body_fb)) {
                events_log(EVENT_WARNING, "Body no-flow detected");
                ctrl_dose_stop();
            }
        }
        if (cube >= corrected(BODY_END_T, p_hpa)) {
            if (eq_lock_phase(&s_eq) != EQ_IDLE) eq_lock_release(&s_eq);
            s_stage = ST_TAILS;
            volume_feedback_begin(&s_tails_fb);
            events_log(EVENT_STAGE_CHANGE, "-> TAILS");
        }
        break;
    }

    case ST_TAILS:
        // EQ_LOCK исключён на хвостах: равновесие намеренно нарушается.
        st->eq_lock_active = false;
        st->eq_lock_phase = (uint8_t)EQ_IDLE;
        ctrl_heater_set_percent(70.0f);
        if (has_reflux) {
            reflux_set_ratio(1.0f);
            reflux_enable(true);
        }
        if (has_valves) valve_set_pct(VALVE_TAILS, 25.0f);
        {
            float flow = control_config_get_flow_ml_min();
            float est = flow * 0.25f * dt / 60.0f;
            float measured = volume_feedback_ml(&s_tails_fb);
            if (measured >= 0.0f) st->tails_collected_ml = measured;
            else st->tails_collected_ml += est;
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            volume_feedback_tick(&s_tails_fb, has_valves, now_ms);
            st->volume_guard_active = true;
            st->volume_no_flow = volume_feedback_no_flow(&s_tails_fb);
            st->volume_overflow = volume_feedback_overflow(&s_tails_fb);
            if (volume_feedback_overflow(&s_tails_fb)) {
                events_log(EVENT_WARNING, "Tails overflow guard triggered");
                if (has_valves) valve_set_pct(VALVE_TAILS, 0.0f);
            }
            if (volume_feedback_no_flow(&s_tails_fb)) {
                events_log(EVENT_WARNING, "Tails no-flow detected");
                if (has_valves) valve_set_pct(VALVE_TAILS, 0.0f);
            }
        }
        if (cube >= corrected(st->tail_stop_temp, p_hpa)) {
            s_stage = ST_COOLDOWN;
            events_log(EVENT_STAGE_CHANGE, "-> COOLDOWN");
        }
        break;

    case ST_COOLDOWN:
        ctrl_heater_force_off();
        ctrl_valves_close_all();
        if (has_reflux) reflux_enable(false);
        if (cube < 40.0f) s_stage = ST_DONE;
        break;

    case ST_DONE:
        if (has_cooling) hal_relay_set(RELAY_COOLING_PUMP, false);
        if (has_cooling) hal_relay_set(RELAY_WATER_VALVE, false);
        st->is_running = false;
        break;

    default:
        break;
    }

    s_last_col = col;
    st->stage = s_stage;
    st->flood_active = s_flooded || flood_detector_active(&s_flood_det);
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
    return s_stage;
}

static void r_stop(void) {
    ctrl_heater_force_off();
    ctrl_dose_stop();
    ctrl_valves_close_all();
    hal_relay_all_off();
    if (hw_has_device(HW_DEV_REFLUX)) reflux_enable(false);
}

const process_mode_t MODE_RECTIFY = {
    .name = "Ректификация", .id = "rectify",
    .init = r_init, .update = r_update, .stop = r_stop,
};
