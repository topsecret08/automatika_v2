#include "ctrl_safety.h"
#include "ctrl_heater.h"
#include "ctrl_dose.h"
#include "alcohol_balance.h"
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
#include "hal/baro.h"
#include "hal/flow_sensor.h"
#include "hal/pressure_sensor.h"
#include "level/level_sensor.h"
#include "web/telegram_notify.h"
#include "config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Период safety-задачи. Все проверки — неблокирующие: датчики читаются
// из кэша (1-Wire опрашивается отдельной задачей), I2C-барометр — из
// своей задачи, проток — из счётчика импульсов.
#define SAFETY_PERIOD_MS 100

// Сколько времени без протока допускается при включённом нагреве.
#define NO_FLOW_GRACE_MS 15000u

static volatile bool s_ok = true;
static uint32_t s_no_flow_ms = 0;

/* edge-детекторы для уведомлений, чтобы не спамить Telegram */
static bool s_level_low_last = false;
static bool s_level_over_last = false;
static bool s_run_done_notified = false;

void ctrl_safety_init(void) { s_ok = true; s_no_flow_ms = 0; }

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
    st->emergency_reason[sizeof(st->emergency_reason) - 1] = '\0';
    st->is_running = false;

    events_log(EVENT_EMERGENCY, "SAFETY: %s", reason);
    telegram_notify_alert("АВАРИЯ", reason);
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
    s_no_flow_ms = 0;
    st->emergency = false;
    st->emergency_reason[0] = '\0';
    events_log(EVENT_INFO, "Safety reset");
}

// Кнопка E-STOP обрабатывается здесь, а не в ISR: ISR только фиксирует
// фронт, а обесточивание исполнителей и уведомление выполняются в задаче.
static bool s_estop_latched = false;

bool ctrl_safety_estop_latched(void) { return s_estop_latched; }

void ctrl_safety_task(void *arg) {
    (void)arg;
    wd_register("safety", 2000, true);

    while (1) {
        WD_FEED("safety");

        // ---- E-STOP: наивысший приоритет проверки ----
        if (estop_is_pressed()) {
            if (!s_estop_latched) {
                s_estop_latched = true;
                events_log(EVENT_EMERGENCY, "SAFETY: E-STOP (hardware)");
                telegram_notify_alert("АВАРИЯ", "Нажата кнопка E-STOP!");
            }
            ctrl_safety_trigger("E-STOP");
        } else if (s_estop_latched) {
            // Кнопку отпустили — снимаем защёлку, но система не запускается
            // автоматически: требуется ctrl_safety_reset().
            s_estop_latched = false;
            events_log(EVENT_WARNING, "SAFETY: E-STOP released");
            telegram_notify_alert("ИНФО", "Кнопка E-STOP отпущена, нужен сброс аварии");
        }

        // Быстрые чтения из кэша; 1-Wire не блокирует safety-задачу.
        float cube = hal_1wire_read_fast(0);
        float col  = hal_1wire_read_fast(1);
        float pres = column_pressure_bar();
        bool  running = state_get()->is_running;

        if (cube > SAFE_MAX_CUBE_T)   ctrl_safety_trigger("OVERHEAT_CUBE");
        if (col > SAFE_MAX_COLUMN_T)  ctrl_safety_trigger("OVERHEAT_COLUMN");
        if (pres > SAFE_MAX_PRESSURE) ctrl_safety_trigger("OVERPRESSURE");

        // -127 °C — нет ответа от DS18B20; при работающем процессе это
        // означает потерю контроля температуры.
        if (running && (cube <= -50.0f || col <= -50.0f)) {
            ctrl_safety_trigger("SENSOR_FAIL");
        }

        // ---- Проток охлаждающей воды ----
        // Проверяем только для процессов с водяным охлаждением и только
        // если датчик сконфигурирован (плата без расходомера не должна
        // уходить в аварию).
        process_type_t ptype = state_get()->type;
        bool water_cooled = (ptype == PROCESS_DISTILL || ptype == PROCESS_RECTIFY ||
                             ptype == PROCESS_DOUBLE || ptype == PROCESS_GABRIEL);
        if (water_cooled && flow_is_present()) {
            if (running && ctrl_heater_get_percent() > 0.0f && !flow_is_ok()) {
                s_no_flow_ms += SAFETY_PERIOD_MS;
                if (s_no_flow_ms >= NO_FLOW_GRACE_MS) {
                    ctrl_safety_trigger("NO_FLOW");
                }
            } else {
                s_no_flow_ms = 0;
            }
        } else {
            s_no_flow_ms = 0;
        }

        // ---- Пролив ----
        if (flags_get_by_key("spill_sensor")) {
            float spill = hal_adc_spill_voltage();
            if (spill > 2.5f) ctrl_safety_trigger("SPILL_DETECTED");
        }

        // ---- Насос охлаждения: нагрев без насоса недопустим ----
        // Для процессов с водяным охлаждением safety сам включает насос,
        // если режим этого не сделал (например, двойная перегонка).
        if (water_cooled && state_get()->is_running &&
            ctrl_heater_get_percent() > 0 &&
            !hal_relay_get(RELAY_COOLING_PUMP)) {
            hal_relay_set(RELAY_COOLING_PUMP, true);
            events_log(EVENT_WARNING, "Cooling pump enabled by safety");
        }

        // ---- Датчик уровня ----
        level_sensor_poll();
        if (level_is_too_low()) {
            ctrl_heater_set_percent(0);       /* сухой ход: снимаем нагрев */
            if (s_level_low_last != true) {
                s_level_low_last = true;
                events_log(EVENT_WARNING, "LEVEL low: heating blocked");
                telegram_notify_alert("ВНИМАНИЕ", "Уровень жидкости низкий — нагрев заблокирован");
            }
        } else {
            s_level_low_last = false;
        }
        if (level_is_overflow()) {
            if (s_level_over_last != true) {
                s_level_over_last = true;
                telegram_notify_alert("АВАРИЯ", "Переполнение ёмкости — процесс остановлен");
            }
            ctrl_safety_trigger("LEVEL_OVERFLOW");
        } else {
            s_level_over_last = false;
        }

        // ---- Завершение процесса ----
        if (running && !state_get()->is_running && !s_run_done_notified) {
            s_run_done_notified = true;
            telegram_notify_alert("ГОТОВО", "Процесс завершён");
        }
        if (running) s_run_done_notified = false;

        // Остаток спирта: раз в 500 мс по T куба и абсолютному давлению.
        static int64_t s_last_abv_us = 0;
        int64_t now_abv = esp_timer_get_time();
        if (s_last_abv_us == 0 || (now_abv - s_last_abv_us) >= 500000LL) {
            s_last_abv_us = now_abv;
            float p_hpa = baro_get_pressure_hpa();
            // Прогнать конвейер независимо от того, вызывали ли его режимы.
            (void)hal_1wire_read_compensated(0, p_hpa);
            (void)hal_1wire_read_compensated(1, p_hpa);
            abv_update(cube, p_hpa);
        }

        vTaskDelay(pdMS_TO_TICKS(SAFETY_PERIOD_MS));
    }
}
