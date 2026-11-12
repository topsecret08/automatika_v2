#include "mode_base.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const process_mode_t *s_cur = NULL;
static int64_t s_last_us = 0;

void mode_set(const process_mode_t *m) {
    if (!m) return;
    if (s_cur && s_cur->stop) s_cur->stop();
    ctrl_dose_stop();
    s_cur = m;
    events_log(EVENT_INFO, "Mode: %s", m->name);
    if (m->init) m->init();
}

const process_mode_t* mode_get(void) { return s_cur; }

void mode_task(void *arg) {
    (void)arg;
    wd_register("mode", 3000, true);
    s_last_us = esp_timer_get_time();

    /* Порядок шага: сначала режим (он двигает этап и нагрев), затем
     * снимок времени — так dt каждой итерации расходуется ровно один раз. */
    while (1) {
        WD_FEED("mode");

        int64_t now = esp_timer_get_time();
        float dt = (now - s_last_us) / 1e6f;
        s_last_us = now;
        if (dt < 0.1f || dt > 5.0f) dt = 1.0f;

        if (s_cur && s_cur->update && state_get()->is_running &&
            !state_get()->is_paused && ctrl_safety_ok()) {
            s_cur->update(dt);
        }
        state_update_snapshot(dt);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}