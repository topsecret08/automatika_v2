#include "ctrl_heater.h"
#include "regulator_driver.h"
#include "hal/hal_dac.h"
#include "hal/hal_1wire.h"
#include "control/ctrl_safety.h"
#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "core/estop.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static volatile float s_pct = 0;
static volatile bool  s_trend_cut = false;

void ctrl_heater_init(void) {
    hal_dac_init();
    hal_dac_set_percent(0);
    regulator_init_from_config();
}

// Упреждающий анализ: если t° куба растёт (temp_filter_is_rising), снижаем
// мощность на HEATER_TREND_CUT%, но не ниже TREND_GUARD_MIN_PCT.
static float trend_guard(float pct) {
    s_trend_cut = false;
    if (pct <= 0.0f) return 0.0f;
    if (hal_1wire_count() <= 0) return pct;

    if (hal_1wire_rising(0) && pct > TREND_GUARD_MIN_PCT) {
        float cut = pct * HEATER_TREND_CUT;
        if (cut < TREND_GUARD_MIN_PCT) cut = TREND_GUARD_MIN_PCT;
        if (cut < pct) {
            s_trend_cut = true;
            return cut;
        }
    }
    return pct;
}

void ctrl_heater_set_percent(float pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    // Инвариант безопасности: нагрев невозможен при E-STOP или аварии,
    // как бы его ни запрашивали (UI, REST, MQTT, Lua, режим).
    if (estop_is_pressed()) pct = 0;
    else if (pct > 0 && !ctrl_safety_ok()) pct = 0;

    float out = trend_guard(pct);

    s_pct = out;
    if (regulator_is_external()) {
        hal_dac_set_percent(0);
        regulator_set_power(out);
    } else {
        hal_dac_set_percent(out);
    }
    state_get()->power_percent = out;
}

float ctrl_heater_get_percent(void) { return s_pct; }

bool ctrl_heater_trend_cut_active(void) { return s_trend_cut; }

bool ctrl_heater_service_set(float pct) {
    if (estop_is_pressed() || !ctrl_safety_ok()) return false;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    s_pct = pct;
    if (regulator_is_external()) {
        hal_dac_set_percent(0);
        regulator_set_power(pct);
    } else {
        hal_dac_set_percent(pct);
    }
    state_get()->power_percent = pct;
    return true;
}

void ctrl_heater_task(void *arg) {
    (void)arg;
    wd_register("heater", 3000, false);

    bool last_cut = false;
    while (1) {
        WD_FEED("heater");

        bool cut = s_trend_cut;
        if (cut != last_cut) {
            last_cut = cut;
            events_log(EVENT_INFO, "Heater: trend guard %s (slope %.1f °C/min)",
                       cut ? "ON" : "off", hal_1wire_slope(0));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
