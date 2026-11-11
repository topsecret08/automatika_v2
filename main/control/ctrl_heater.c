#include "ctrl_heater.h"
#include "hal/hal_dac.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static float s_pct = 0;

void ctrl_heater_init(void) {
    hal_dac_init();
    hal_dac_set_percent(0);
}

void ctrl_heater_set_percent(float pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    s_pct = pct;
    hal_dac_set_percent(pct);
    state_get()->power_percent = pct;
}

float ctrl_heater_get_percent(void) { return s_pct; }

void ctrl_heater_task(void *arg) {
    (void)arg;
    wd_register("heater", 3000, false);
    while (1) {
        WD_FEED("heater");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}