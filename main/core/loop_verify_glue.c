#include "loop_verify.h"
#include "control/safety_gate.h"
#include "event_bus.h"
#include "verify/shadow.h"
#include "verify/verify_db.h"
#include "esp_timer.h"

static uint32_t s_last_aggregate;
static uint32_t s_last_detail;

void lv_escalate(const lv_result_t *r) {
    if (!r) return;

    uint32_t detail = r->sensor_flags | r->program_flags |
                      r->actuator_flags;

    // Логируем и эскалируем только смену состояния. Иначе WARN на каждом
    // такте (2 Гц) вытесняет журнал и пишет на SD.
    if (!lv_escalation_changed(r->aggregate, detail,
                               &s_last_aggregate, &s_last_detail))
        return;
    if (r->aggregate == 0) return;

    bus_event_t ev = {
        .prio = r->aggregate >= 4u ? EV_CRITICAL : EV_MINOR,
        .type = "LOOP_VERIFY",
        .source_id = (uint8_t)r->aggregate,
        .msg = "loop verification",
        .ts_us = esp_timer_get_time(),
    };
    event_bus_publish(&ev);
    (void)safety_gate_submit_flags(r->aggregate, detail);

    // Запись в SQLite — после эскалации, чтобы медленная SD не задерживала
    // аварийную реакцию.
    int64_t run = shadow_current_run();
    if (run > 0) {
        (void)verify_db_log_loop(run, esp_timer_get_time() / 1000,
                                 r->aggregate, detail,
                                 r->latency_e2e_us);
    }
}

void lv_tick(int64_t sensor_us,
             const lv_sensor_frame_t *s, size_t n_s,
             const lv_program_frame_t *p,
             const lv_actuator_frame_t *a, size_t n_a,
             lv_result_t *out) {
    lv_tick_at(esp_timer_get_time(), sensor_us, s, n_s, p, a, n_a, out);
    lv_escalate(out);
}
