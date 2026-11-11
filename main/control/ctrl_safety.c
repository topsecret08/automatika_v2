#include "ctrl_safety.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "hal/hal_1wire.h"
#include "hal/hal_adc.h"
#include "hal/hal_dac.h"
#include "hal/hal_relay.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static bool s_ok = true;

void ctrl_safety_init(void) { s_ok = true; }

bool ctrl_safety_ok(void) { return s_ok; }

void ctrl_safety_trigger(const char *reason) {
    if (!s_ok) return;
    s_ok = false;

    hal_dac_set_percent(0);
    hal_relay_all_off();

    process_state_t *st = state_get();
    st->emergency = true;
    strncpy(st->emergency_reason, reason, sizeof(st->emergency_reason) - 1);
    st->is_running = false;

    events_log(EVENT_EMERGENCY, "SAFETY: %s", reason);
}

void ctrl_safety_reset(void) {
    s_ok = true;
    state_get()->emergency = false;
    events_log(EVENT_INFO, "Safety reset");
}

void ctrl_safety_task(void *arg) {
    (void)arg;
    wd_register("safety", 2000, true);

    while (1) {
        WD_FEED("safety");

        float cube = hal_1wire_read(0);
        float col  = hal_1wire_read(1);
        float pres = hal_adc_pressure_bar();

        if (cube > SAFE_MAX_CUBE_T)     ctrl_safety_trigger("OVERHEAT_CUBE");
        if (col > SAFE_MAX_COLUMN_T)    ctrl_safety_trigger("OVERHEAT_COLUMN");
        if (pres > SAFE_MAX_PRESSURE)   ctrl_safety_trigger("OVERPRESSURE");
        if (cube < -50.0f && cube > -100.0f) ctrl_safety_trigger("SENSOR_FAIL");

        // Датчик разлития
        if (flags_get_by_key("spill_sensor")) {
            float spill = hal_adc_spill_voltage();
            if (spill > 2.5f) ctrl_safety_trigger("SPILL_DETECTED");
        }

        // Насос охлаждения должен работать при нагреве
        extern float ctrl_heater_get_percent(void);
        if (ctrl_heater_get_percent() > 0 && !hal_relay_get(RELAY_COOLING_PUMP)) {
            // только если процесс идёт
            if (state_get()->is_running)
                ctrl_safety_trigger("COOLING_PUMP_OFF");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}