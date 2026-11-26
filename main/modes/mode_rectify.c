#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/control_config.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/flow_sensor.h"
#include "hal/baro.h"
#include "hal/pressure_sensor.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_forecast.h"
#include "control/fraction_periodic.h"
#include "control/volume_feedback.h"
#include "control/ctrl_scale.h"
#include "control/reflux.h"
#include "core/stab_detect.h"
#include "esp_timer.h"
#include "config.h"
#include <math.h>

static proc_stage_t s_stage = ST_IDLE;
static float s_stab_time = 0;
static float s_head_stable_time = 0;
static float s_last_col = 0;
static float s_flood_time = 0;
static float s_flood_hold = 0;
static bool  s_flooded = false;
static stab_detector_t s_stab;
static volume_feedback_t s_heads_fb;
static volume_feedback_t s_body_fb;
static volume_feedback_t s_tails_fb;

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
    s_flood_time = 0;
    s_flood_hold = 0;
    s_flooded = false;
    stab_init(&s_stab, 0.1f, 0.01f, 60.0f, 3600.0f,
              esp_timer_get_time());
    volume_feedback_begin(&s_heads_fb);
    volume_feedback_begin(&s_body_fb);
    volume_feedback_begin(&s_tails_fb);
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
    s_last_col = col;
    return s_head_stable_time > STAB_TIME;
}

static float corrected(float t, float p_hpa) {
    return baro_correct_boiling_point(t, p_hpa);
}

// Захлёб: рост давления в колонне или резкое падение температуры царги
// при заметной мощности. Логика не срабатывает, если датчик давления
// не отмечен в профиле.
static bool flood_detected(float col, float dt, float p_bar, bool has_pres) {
    bool pressure_high = has_pres && p_bar > FLOOD_PRESSURE;
    bool temp_drop = (s_last_col > 0.0f) &&
                     (s_last_col - col > FLOOD_DROP_T) &&
                     ctrl_heater_get_percent() > 30.0f;
    if (pressure_high || temp_drop) {
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
        state_get()->stage = ST_ERROR;
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
            cooling_valve_set(true);
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
        ctrl_heater_set_percent(50.0f);
        if (has_reflux) {
            // На стабилизации — максимальный возврат флегмы.
            reflux_set_ratio(8.0f);
            reflux_enable(true);
        }
        s_stab_time += dt;
        {
            float target = st->target_column_t > 0.0f
                               ? corrected(st->target_column_t, p_hpa)
                               : corrected(HEADS_END_T, p_hpa);
            bool converged = stab_tick(&s_stab, col, target,
                                       esp_timer_get_time());
            if ((flags_get_by_key("autodetect_heads") &&
                 auto_heads_done(col, dt)) || converged) {
                s_stage = ST_HEADS;
                volume_feedback_begin(&s_heads_fb);
                events_log(EVENT_STAGE_CHANGE, "-> HEADS (stab)");
            }
        }
        break;

    case ST_HEADS:
        ctrl_heater_set_percent(45.0f);
        if (has_reflux) {
            reflux_set_ratio(8.0f);
            reflux_enable(true);
        }
        if (has_valves) valve_set_pct(VALVE_HEADS, 5.0f);
        {
            float flow = control_config_get_flow_ml_min();
            float est = flow * 0.05f * dt / 60.0f;
            float measured = volume_feedback_ml(&s_heads_fb);
            if (measured >= 0.0f) st->heads_collected_ml = measured;
            else st->heads_collected_ml += est;
        }
        if (st->heads_collected_ml > st->target_heads_ml ||
            col >= corrected(HEADS_END_T, p_hpa)) {
            s_stage = ST_BODY;
            volume_feedback_begin(&s_body_fb);
            events_log(EVENT_STAGE_CHANGE, "-> BODY");
        }
        break;

    case ST_BODY: {
        float rate = st->body_rate_ml_min;
        float target_col = st->target_column_t > 0.0f
                               ? st->target_column_t : HEADS_END_T;
        // Каскад: отклонение температуры колонны корректирует скорость.
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
            if ((cube - BODY_END_T) > -dist &&
                ctrl_forecast_rate() > 0.05f) {
                rate *= (1.0f - st->predec_step);
                st->predec_count++;
            }
        }
        if (hal_1wire_rising(hw_temp_index(HW_TEMP_CUBE)))
            rate *= PREDEC_TREND_CUT;

        ctrl_heater_set_percent(50.0f);
        if (has_reflux) {
            reflux_set_ratio(REFLUX_RATIO_DEFAULT);
            reflux_enable(true);
        }
        ctrl_dose_process(rate, dt, VALVE_BODY);
        {
            float measured = volume_feedback_ml(&s_body_fb);
            if (measured >= 0.0f) st->body_collected_ml = measured;
            else st->body_collected_ml += rate * dt / 60.0f;
        }
        if (cube >= corrected(BODY_END_T, p_hpa)) {
            s_stage = ST_TAILS;
            volume_feedback_begin(&s_tails_fb);
            events_log(EVENT_STAGE_CHANGE, "-> TAILS");
        }
        break;
    }

    case ST_TAILS:
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
        if (has_cooling) cooling_valve_set(false);
        st->is_running = false;
        break;

    default:
        break;
    }

    s_last_col = col;
    st->stage = s_stage;
    {
        float collected = st->heads_collected_ml + st->body_collected_ml + st->tails_collected_ml;
        // Индикатор весов: после стабилизации/выхода на рабочий режим.
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
