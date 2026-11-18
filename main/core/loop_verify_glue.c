#include "loop_verify.h"
#include "control/safety_gate.h"
#include "event_bus.h"
#include "esp_timer.h"

void lv_escalate(const lv_result_t *r) {
    if (!r || r->aggregate == 0) return;
    bus_event_t ev = {
        .prio = r->aggregate >= 4u ? EV_CRITICAL : EV_MINOR,
        .type = "LOOP_VERIFY",
        .source_id = (uint8_t)r->aggregate,
        .msg = "loop verification",
        .ts_us = esp_timer_get_time(),
    };
    event_bus_publish(&ev);
    (void)safety_gate_submit_flags(r->aggregate,
                                   r->sensor_flags | r->program_flags |
                                   r->actuator_flags);
}

void lv_tick(int64_t sensor_us,
             const lv_sensor_frame_t *s, size_t n_s,
             const lv_program_frame_t *p,
             const lv_actuator_frame_t *a, size_t n_a,
             lv_result_t *out) {
    lv_tick_at(esp_timer_get_time(), sensor_us, s, n_s, p, a, n_a, out);
    lv_escalate(out);
}
