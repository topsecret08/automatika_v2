#include "valve_calibration.h"
#include "core/valve_config.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static valve_calib_session_t s_sessions[3];
static bool s_active[3] = {false, false, false};

void valve_calib_init(void) {
    memset(s_sessions, 0, sizeof(s_sessions));
}

void valve_calib_start(int idx, float expected_ml) {
    if (idx < 0 || idx > 2) return;
    valve_calib_session_t *s = &s_sessions[idx];
    memset(s, 0, sizeof(*s));
    s->valve_idx = idx;
    s->start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    s->active = true;
    s_active[idx] = true;
    events_log(EVENT_INFO, "CALIB start valve=%d expected=%.1f ml", idx, expected_ml);
}

void valve_calib_record_open(int idx, uint32_t duration_ms) {
    if (idx < 0 || idx > 2) return;
    valve_calib_session_t *s = &s_sessions[idx];
    if (!s->active) return;
    s->open_count++;
    s->total_open_ms += duration_ms;
    events_log(EVENT_INFO, "CALIB open #%lu, %lu ms",
               s->open_count, duration_ms);
}

void valve_calib_finish(int idx, float actual_ml) {
    if (idx < 0 || idx > 2) return;
    valve_calib_session_t *s = &s_sessions[idx];
    if (!s->active) return;

    s->measured_ml = actual_ml;
    if (s->total_open_ms > 0)
        s->ml_per_hour = actual_ml * 3600000.0f / s->total_open_ms;
    if (s->open_count > 0)
        s->ml_per_open = actual_ml / s->open_count;

    valve_config_t *v = valve_config_get(idx);
    if (v) {
        v->ml_per_hour = s->ml_per_hour;
        v->calibrated = true;
        v->last_calib_ms = (uint32_t)(esp_timer_get_time() / 1000);
        valve_config_save();
    }

    s->active = false;
    s_active[idx] = false;

    events_log(EVENT_INFO,
        "CALIB done valve=%d: opens=%lu total=%lu ms vol=%.1f ml "
        "rate=%.1f ml/h per_open=%.1f ml",
        idx, s->open_count, s->total_open_ms, actual_ml,
        s->ml_per_hour, s->ml_per_open);
}

void valve_calib_cancel(int idx) {
    if (idx < 0 || idx > 2) return;
    s_active[idx] = false;
    s_sessions[idx].active = false;
    events_log(EVENT_WARNING, "CALIB cancelled valve=%d", idx);
}

valve_calib_session_t* valve_calib_get(int idx) {
    return (idx >= 0 && idx < 3) ? &s_sessions[idx] : NULL;
}

float valve_calib_estimate_flow(int idx) {
    valve_config_t *v = valve_config_get(idx);
    return v ? v->ml_per_hour : 0;
}

float valve_calib_estimate_volume(int idx, uint32_t elapsed_ms) {
    float rate = valve_calib_estimate_flow(idx);
    return rate * elapsed_ms / 3600000.0f;
}

bool valve_calib_is_active(int idx) {
    return (idx >= 0 && idx < 3) ? s_active[idx] : false;
}

void valve_calib_task(void *arg) {
    (void)arg;
    wd_register("vcalib", 30000, false);

    while (1) {
        WD_FEED("vcalib");

        for (int i = 0; i < 3; i++) {
            valve_calib_session_t *s = &s_sessions[i];
            if (s->active && s->open_count > 0) {
                uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
                if (now - s->start_ms > 1800000) {
                    events_log(EVENT_WARNING, "CALIB timeout valve=%d", i);
                    s->active = false;
                    s_active[i] = false;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}