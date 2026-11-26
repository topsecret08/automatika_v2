#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_stepper.h"
#include "control/ctrl_scale.h"
#include "modes/beer_presets.h"

#include <math.h>

static const beer_step_t *s_recipe = NULL;
static int s_step_count = 0;
static int s_step = 0;
static float s_step_time = 0;
static bool s_step_started = false;

static bool step_is_heat(uint8_t type) {
    return type == BST_MASH || type == BST_REST || type == BST_HOP ||
           type == BST_BOIL;
}

static void stirrer_set(bool on) {
    if (hw_has_device(HW_DEV_STIRRER))
        (void)ctrl_stepper_stirrer(on, false, 60.0f);
}

static void cooling_set(bool on) {
    if (!hw_has_device(HW_DEV_COOLING_PUMP)) return;
    hal_relay_set(RELAY_COOLING_PUMP, on);
    cooling_valve_set(on);
}

static void pump_set(bool on) {
    if (!hw_has_device(HW_DEV_COOLING_PUMP)) return;
    hal_relay_set(RELAY_COOLING_PUMP, on);
}

static void step_enter(const beer_step_t *step) {
    s_step_started = true;
    s_step_time = 0;
    float volume = scale_is_ready() ? scale_get_total_ml() : -1.0f;

    events_log(EVENT_STAGE_CHANGE, "Beer: %s (%.1f C, %us)",
               step->name, (double)step->temp,
               (unsigned)step->duration_sec);
    if (volume >= 0.0f)
        events_log(EVENT_INFO, "Beer: volume %.0f ml", (double)volume);

    switch (step->type) {
    case BST_MASH:
    case BST_REST:
        stirrer_set(true);
        cooling_set(false);
        pump_set(false);
        break;
    case BST_BOIL:
    case BST_HOP:
        stirrer_set(false);
        cooling_set(false);
        pump_set(false);
        break;
    case BST_CHILL:
        stirrer_set(false);
        ctrl_heater_force_off();
        cooling_set(true);
        break;
    case BST_XFER:
        stirrer_set(false);
        cooling_set(false);
        pump_set(true);
        break;
    default:
        break;
    }
}

static proc_stage_t b_init(void) {
    s_step = 0;
    s_step_time = 0;
    s_step_started = false;

    s_recipe = beer_preset_get(beer_preset_active(), &s_step_count);
    if (!s_recipe || s_step_count == 0) return ST_ERROR;

    ctrl_pid_init_ext(15.0f, 0.5f, 2.0f, s_recipe[0].temp, 2000.0f, 0);
    ctrl_pid_set_limits(0, 100);
    ctrl_pid_set_sample_time(2000);
    events_log(EVENT_INFO, "Beer: preset %d, %d steps",
               beer_preset_active(), s_step_count);
    return ST_PRECHECK;
}

static proc_stage_t b_update(float dt) {
    s_step_time += dt;
    process_state_t *st = state_get();
    float t = hw_temp_read(HW_TEMP_CUBE);
    if (!isfinite(t)) t = hw_temp_read(HW_TEMP_TANK);
    if (isfinite(t)) st->cube_temp = t;

    if (s_step >= s_step_count) {
        ctrl_heater_force_off();
        cooling_set(false);
        pump_set(false);
        stirrer_set(false);
        st->is_running = false;
        st->stage = ST_DONE;
        scale_update_state(st, true, 0.0f);
        return ST_DONE;
    }

    const beer_step_t *cur = &s_recipe[s_step];
    if (!s_step_started) step_enter(cur);

    if (cur->type == BST_CHILL) {
        if (!hw_has_device(HW_DEV_COOLING_PUMP)) {
            events_log(EVENT_WARNING, "Beer: chill skipped, no cooling");
            s_step++;
            s_step_started = false;
            st->stage = ST_RUNNING;
            scale_update_state(st, true, 0.0f);
            return ST_RUNNING;
        }
        ctrl_heater_force_off();
        cooling_set(true);
        bool reached = !isfinite(t) || t <= cur->temp + 1.0f;
        bool timed = cur->duration_sec > 0 &&
                     s_step_time >= (float)cur->duration_sec;
        if (reached || timed) {
            cooling_set(false);
            s_step++;
            s_step_started = false;
        }
        st->stage = ST_RUNNING;
        scale_update_state(st, true, 0.0f);
        return ST_RUNNING;
    }

    if (cur->type == BST_XFER) {
        if (cur->duration_sec == 0 ||
            s_step_time >= (float)cur->duration_sec) {
            pump_set(false);
            s_step++;
            s_step_started = false;
        }
        st->stage = ST_RUNNING;
        scale_update_state(st, true, 0.0f);
        return ST_RUNNING;
    }

    // Нагрев/пауза/кипячение.
    if (isfinite(t)) {
        float target = cur->temp;
        if (target > 0.0f) {
            float err = target - t;
            if (err > 3.0f && step_is_heat(cur->type)) {
                ctrl_heater_set_percent(100.0f);
            } else if (cur->type == BST_BOIL && target >= 99.0f) {
                ctrl_heater_set_percent(100.0f);
            } else {
                ctrl_pid_set_setpoint(target);
                ctrl_pid_compute_and_apply(t);
            }
        }
    }

    bool wait_temp = cur->wait_for_temp && isfinite(t) &&
                     fabsf(t - cur->temp) > 1.0f;
    bool duration_done = cur->duration_sec == 0 ||
                         s_step_time >= (float)cur->duration_sec;
    if (!wait_temp && duration_done) {
        s_step++;
        s_step_started = false;
    }

    st->stage = ST_RUNNING;
    scale_update_state(st, true, 0.0f);
    return ST_RUNNING;
}

static void b_stop(void) {
    ctrl_heater_force_off();
    cooling_set(false);
    pump_set(false);
    stirrer_set(false);
}

const process_mode_t MODE_BEER = {
    .name = "Пивоварня", .id = "beer",
    .init = b_init, .update = b_update, .stop = b_stop,
};
