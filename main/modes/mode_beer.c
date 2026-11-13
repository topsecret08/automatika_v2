#include "mode_base.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "hal/hal_1wire.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_pid.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "modes/beer_presets.h"

static const beer_step_t *s_recipe = NULL;
static int s_step_count = 0;
static int s_step = 0;
static float s_step_time = 0;

static proc_stage_t b_init(void) {
    s_step = 0;
    s_step_time = 0;

    s_recipe = beer_preset_get(0, &s_step_count);   // Эль по умолчанию
    if (!s_recipe || s_step_count == 0) return ST_ERROR;

    ctrl_pid_init_ext(15.0f, 0.5f, 2.0f, s_recipe[0].temp, 2000.0f, 0);
    ctrl_pid_set_limits(0, 100);
    ctrl_pid_set_sample_time(2000);
    events_log(EVENT_INFO, "Beer: %d steps", s_step_count);
    return ST_PRECHECK;
}

static proc_stage_t b_update(float dt) {
    s_step_time += dt;
    float t = hal_1wire_read(0);
    state_get()->cube_temp = t;

    if (s_step >= s_step_count) {
        state_get()->is_running = false;
        return ST_DONE;
    }

    const beer_step_t *cur = &s_recipe[s_step];
    ctrl_pid_set_setpoint(cur->temp);
    ctrl_pid_compute_and_apply(t);

    if (cur->type == 3) {   // STEP_CHILL
        valve_set_pct(VALVE_BODY, 100.0f);
        if (t < cur->temp + 1.0f) { s_step++; s_step_time = 0; }
    } else if (cur->duration_sec > 0) {
        if (s_step_time > cur->duration_sec) {
            s_step++;
            s_step_time = 0;
            if (s_step < s_step_count)
                events_log(EVENT_STAGE_CHANGE, "→ %s", s_recipe[s_step].name);
        }
    }

    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void b_stop(void) { ctrl_heater_force_off(); }

const process_mode_t MODE_BEER = {
    .name = "Пивоварня", .id = "beer",
    .init = b_init, .update = b_update, .stop = b_stop,
};