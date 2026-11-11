#include "mode_base.h"
#include "core/state.h"
#include "core/events.h"
#include "hal/hal_1wire.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include <math.h>

static int s_pass = 1;

static proc_stage_t db_init(void) {
    s_pass = 1;
    return ST_PRECHECK;
}

static proc_stage_t db_update(float dt) {
    (void)dt;
    float cube = hal_1wire_read(0);
    state_get()->cube_temp = cube;

    ctrl_heater_set_percent(70.0f);
    valve_set_pct(VALVE_BODY, s_pass == 1 ? 40.0f : 20.0f);

    float stop_t = (s_pass == 1) ? 98.0f : 92.0f;
    if (cube > stop_t && s_pass == 1) {
        s_pass = 2;
        events_log(EVENT_STAGE_CHANGE, "→ Pass 2");
    } else if (cube > stop_t && s_pass == 2) {
        state_get()->is_running = false;
        return ST_DONE;
    }
    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

static void db_stop(void) {
    ctrl_heater_set_percent(0);
    ctrl_valves_close_all();
}

// Метод Габриэля
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