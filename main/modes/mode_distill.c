#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/control_config.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "hal/baro.h"
#include "hal/flow_sensor.h"
#include "hal/pressure_sensor.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_forecast.h"
#include "control/fraction_logic.h"
#include "control/fraction_periodic.h"
#include "control/reflux.h"
#include "web/telegram_notify.h"
#include "config.h"
#include <math.h>

static proc_stage_t s_stage = ST_IDLE;
static float s_stage_t = 0;
static int s_predec_logged = -1;
static fraction_t s_frac = FRAC_HEADS;
static bool s_reflux_on = false;

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
            reflux_enable(true);
        }
    } else {
        if (s_reflux_on) {
            s_reflux_on = false;
            reflux_enable(false);
        }
    }
}

static proc_stage_t d_update(float dt) {
    s_stage_t += dt;

    float cube = hal_1wire_read(0);
    float col  = hal_1wire_read(1);
    float pres_hpa = baro_get_pressure_hpa();
    float cube_corr = baro_correct_boiling_point(cube, pres_hpa);

    ctrl_forecast_update(cube, dt);

    process_state_t *st = state_get();
    st->cube_temp = cube;
    st->column_temp = col;
    st->pressure_bar = column_pressure_bar();

    // Периодический отбор — альтернативная логика отбора. Когда она включена,
    // штатный fraction_update/valve/dose не должен работать: иначе два
    // алгоритма одновременно управляют одним клапаном. Нагрев, reflux и
    // охлаждение берёт на себя fsel_mode_tick().
    if (fsel_mode_tick(st, 40.0f, BODY_POWER, TAIL_POWER)) return st->stage;

    float rate = st->body_rate_ml_min;
    float total_ml = st->heads_collected_ml + st->body_collected_ml +
                     st->tails_collected_ml;

    // Логика фракций работает от скорректированной температуры и объёма.
    fraction_t f = fraction_update(cube_corr, total_ml);
    fraction_apply(f);

    switch (s_stage) {
    case ST_PRECHECK:
        cooling_valve_set(true);
        hal_relay_set(RELAY_COOLING_PUMP, true);
        if (!ctrl_safety_ok()) { s_stage = ST_ERROR; break; }
        s_stage = ST_HEATUP;
        break;

    case ST_HEATUP:
        ctrl_heater_set_percent(PREHEAT_POWER);
        if (cube > 70.0f) { s_stage = ST_STABILIZE; s_stage_t = 0; }
        break;

    case ST_STABILIZE:
        ctrl_heater_set_percent(50.0f);
        if (s_stage_t > 600.0f) {
            s_stage = ST_HEADS;
            events_log(EVENT_STAGE_CHANGE, "-> HEADS");
        }
        break;

    case ST_HEADS:
        ctrl_heater_set_percent(40.0f);
        valve_set_pct(VALVE_HEADS, 20.0f);
        st->heads_collected_ml += 5.0f * dt;

        if (f >= FRAC_BODY ||
            st->heads_collected_ml > st->target_heads_ml) {
            s_stage = ST_BODY;
            events_log(EVENT_STAGE_CHANGE, "-> BODY");
        }
        break;

    case ST_BODY: {
        if (flags_get_by_key("auto92") && cube > 85.0f) {
            float factor = 6.04f - 0.06f * cube;
            if (factor < 0.1f) factor = 0.1f;
            rate *= factor;
        }
        rate *= predec_factor(cube);

        ctrl_heater_set_percent(BODY_POWER);
        ctrl_dose_process(rate, dt, VALVE_BODY);
        if (control_config_get_output_device() == OUTPUT_DEVICE_PUMP)
            st->body_collected_ml += rate * dt / 60.0f;

        if (f >= FRAC_TAILS || cube >= TAIL_START_T) {
            s_stage = ST_TAILS;
            events_log(EVENT_STAGE_CHANGE, "-> TAILS");
        }
        break;
    }

    case ST_TAILS:
        ctrl_heater_set_percent(TAIL_POWER);
        valve_set_pct(VALVE_TAILS, 30.0f);
        st->tails_collected_ml += 15.0f * dt;

        if (f >= FRAC_DONE || cube > st->target_cube_t) {
            s_stage = ST_COOLDOWN;
            events_log(EVENT_STAGE_CHANGE, "-> COOLDOWN");
        }
        break;

    case ST_COOLDOWN:
        ctrl_heater_force_off();
        ctrl_dose_stop();
        ctrl_valves_close_all();
        reflux_enable(false);
        if (cube < 40.0f) s_stage = ST_DONE;
        break;

    case ST_DONE:
        hal_relay_set(RELAY_COOLING_PUMP, false);
        cooling_valve_set(false);
        st->is_running = false;
        break;

    default:
        break;
    }

    st->stage = s_stage;
    return s_stage;
}

static void d_stop(void) {
    ctrl_heater_force_off();
    ctrl_dose_stop();
    ctrl_valves_close_all();
    reflux_enable(false);
    hal_relay_all_off();
}

const process_mode_t MODE_DISTILL = {
    .name = "Дистилляция", .id = "distill",
    .init = d_init, .update = d_update, .stop = d_stop,
};
