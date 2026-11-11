#include "db_logger.h"
#include "db_manager.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

extern int64_t g_current_process_id;

void db_logger_event(event_type_t type, const char *msg) {
    if (!db_is_ready()) return;
    db_event_t e = {
        .ts_ms = esp_timer_get_time() / 1000,
        .type = type,
    };
    strncpy(e.msg, msg, 127);
    db_event_insert(&e);
}

void db_logger_task(void *arg) {
    (void)arg;
    wd_register("dblog", 15000, false);
    while (1) {
        WD_FEED("dblog");
        if (g_current_process_id && db_is_ready()) {
            process_state_t *s = state_get();
            if (s->is_running) {
                db_measurement_t m = {
                    .process_id = g_current_process_id,
                    .ts_ms = esp_timer_get_time() / 1000,
                    .cube_temp = s->cube_temp,
                    .column_temp = s->column_temp,
                    .jacket_temp = s->jacket_temp,
                    .pressure = s->pressure_bar,
                    .power = s->power_percent,
                    .valve_body = s->valve_body_pct,
                    .flow = s->flow_lpm,
                };
                db_meas_insert(g_current_process_id, &m);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}