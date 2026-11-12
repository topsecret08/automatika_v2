#include "ctrl_safety.h"
#include "ctrl_heater.h"
#include "ctrl_dose.h"
#include "core/control_config.h"
#include "core/estop.h"
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

static volatile bool s_ok = true;

void ctrl_safety_init(void) { s_ok = true; }

bool ctrl_safety_ok(void) { return s_ok; }

void ctrl_safety_trigger(const char *reason) {
    if (!s_ok) return;
    s_ok = false;

    hal_dac_set_percent(0);
    hal_relay_all_off();
    ctrl_dose_stop();

    process_state_t *st = state_get();
    st->emergency = true;
    strncpy(st->emergency_reason, reason, sizeof(st->emergency_reason) - 1);
    st->is_running = false;

    events_log(EVENT_EMERGENCY, "SAFETY: %s", reason);
}

void ctrl_safety_reset(void) {
    process_state_t *st = state_get();
    if (st->is_running || ctrl_heater_get_percent() > 0.0f) {
        events_log(EVENT_WARNING,
                   "Safety reset refused: stop the process and cut heating first");
        return;
    }
    if (estop_is_pressed()) {
        events_log(EVENT_WARNING,
                   "Safety reset refused: E-STOP active (release the button first)");
        return;
    }
    s_ok = true;
    st->emergency = false;
    st->emergency_reason[0] = '\0';
    events_log(EVENT_INFO, "Safety reset");
}

// Общая защита: защёлкнута ли авария сейчас. Вызывается и в безопасности,
// и во всех путях включения нагрева (ctrl_heater_set_percent).
static bool s_estop_latched = false;

bool ctrl_safety_estop_latched(void) { return s_estop_latched; }

void ctrl_safety_task(void *arg) {
    (void)arg;
    wd_register("safety", 2000, true);

    while (1) {
        WD_FEED("safety");

        // ---- E-STOP: приоритет выше всех остальных проверок ----
        if (estop_is_pressed()) {
            if (!s_estop_latched) {
                s_estop_latched = true;
                events_log(EVENT_EMERGENCY, "SAFETY: E-STOP (hardware)");
            }
            ctrl_safety_trigger("E-STOP");
        } else if (s_estop_latched) {
            // кнопка отпущена — снимаем защёлку, но авария остаётся до
            // явного ctrl_safety_reset()
            s_estop_latched = false;
            events_log(EVENT_WARNING, "SAFETY: E-STOP released");
        }

        // Быстрые значения из кэша: фоновая задача опрашивает DS18B20.
        float cube = hal_1wire_read(0);
        float col  = hal_1wire_read(1);
        float pres = hal_adc_pressure_bar();
        bool  running = state_get()->is_running;

        if (cube > SAFE_MAX_CUBE_T)     ctrl_safety_trigger("OVERHEAT_CUBE");
        if (col > SAFE_MAX_COLUMN_T)    ctrl_safety_trigger("OVERHEAT_COLUMN");
        if (pres > SAFE_MAX_PRESSURE)   ctrl_safety_trigger("OVERPRESSURE");

        // -127 °C — нет связи с DS18B20; также ловим любой сбой датчика.
        if (running && (cube <= -50.0f || col <= -50.0f)) {
            ctrl_safety_trigger("SENSOR_FAIL");
        }

        // Датчик разлития
        if (flags_get_by_key("spill_sensor")) {
            float spill = hal_adc_spill_voltage();
            if (spill > 2.5f) ctrl_safety_trigger("SPILL_DETECTED");
        }

        // Насос охлаждения должен работать при нагреве
        if (ctrl_heater_get_percent() > 0 && !hal_relay_get(RELAY_COOLING_PUMP)) {
            if (state_get()->is_running)
                ctrl_safety_trigger("COOLING_PUMP_OFF");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
