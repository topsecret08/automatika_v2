#include "mode_base.h"
#include "core/state.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "config.h"

typedef enum { FM_HEAT, FM_COOL, FM_AUTO } fm_mode_t;
static fm_mode_t s_mode = FM_AUTO;
static float s_target = 20.0f;
static float s_hyst = 0.3f;

static proc_stage_t f_init(void) {
    s_mode = FM_AUTO;
    return ST_PRECHECK;
}

static proc_stage_t f_update(float dt) {
    (void)dt;
    float t = hal_1wire_read(4);
    if (t < -100) t = hal_1wire_read(0);
    state_get()->cube_temp = t;

    if (s_mode == FM_HEAT || (s_mode == FM_AUTO && t < s_target - s_hyst)) {
        hal_relay_set(RELAY_WATER_VALVE, false);
        hal_relay_set(RELAY_EXTRA, true);
        if (t > s_target + s_hyst) hal_relay_set(RELAY_EXTRA, false);
    } else if (s_mode == FM_COOL ||
               (s_mode == FM_AUTO && t > s_target + s_hyst)) {
        hal_relay_set(RELAY_EXTRA, false);
        hal_relay_set(RELAY_COOLING_PUMP, true);
        if (t < s_target - s_hyst) hal_relay_set(RELAY_COOLING_PUMP, false);
    }

    state_get()->stage = ST_RUNNING;
    return ST_RUNNING;
}

const process_mode_t MODE_FERMENT = {
    .name = "Брожение", .id = "ferment",
    .init = f_init, .update = f_update, .stop = NULL,
};