#include "shadow.h"

#include "verify_db.h"

#include "core/state.h"
#include "core/watchdog.h"
#include "hal/hal_1wire.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

static const char *TAG = "SHADOW";

#define SHADOW_QUEUE_LEN   128
#define SHADOW_LOG_PERIOD_MS 1000
#define SHADOW_CHANNELS    4

typedef struct {
    verify_trace_t trace;
} shadow_msg_t;

typedef struct {
    char channel[24];
    int64_t last_ms;
} shadow_rate_t;

static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static volatile bool s_armed;
static volatile int64_t s_run_id;
static shadow_rate_t s_rate[SHADOW_CHANNELS];

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static bool rate_allow(const char *channel, int64_t now) {
    for (int i = 0; i < SHADOW_CHANNELS; i++) {
        if (s_rate[i].channel[0] == 0 ||
            strcmp(s_rate[i].channel, channel) == 0) {
            if (s_rate[i].channel[0] == 0) {
                strncpy(s_rate[i].channel, channel, sizeof(s_rate[i].channel) - 1);
            }
            if (now - s_rate[i].last_ms < SHADOW_LOG_PERIOD_MS) return false;
            s_rate[i].last_ms = now;
            return true;
        }
    }
    return false;
}

static void shadow_writer_task(void *arg) {
    (void)arg;
    wd_register("shadow", 15000, false);

    shadow_msg_t msg;
    while (1) {
        WD_FEED("shadow");
        if (xQueueReceive(s_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (verify_db_is_ready() && msg.trace.run_id > 0) {
                verify_db_log_trace(&msg.trace);
            }
        }
    }
}

esp_err_t shadow_init(void) {
    if (!verify_is_enabled()) return ESP_ERR_NOT_SUPPORTED;

    s_queue = xQueueCreate(SHADOW_QUEUE_LEN, sizeof(shadow_msg_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    BaseType_t ok = xTaskCreatePinnedToCore(shadow_writer_task, "shadow_wr",
                                            4096, NULL, 3, &s_task, 0);
    if (ok != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

float shadow_compute(const char *channel, float setpoint, float sensor,
                     float real_out, shadow_fn_t complex_fn) {
    if (!verify_is_enabled() || !s_queue) return real_out;

    shadow_policy_t policy = verify_get_shadow_policy();
    if (policy == SHADOW_OFF) return real_out;

    bool run_now = (policy == SHADOW_AUTO) || s_armed;
    float shadow_out = real_out;
    if (run_now && complex_fn) shadow_out = complex_fn(setpoint, sensor);

    int64_t now = now_ms();
    if (!rate_allow(channel, now)) return real_out;

    if (s_run_id <= 0) s_run_id = shadow_start_run("shadow");

    shadow_msg_t msg = {0};
    msg.trace.run_id = s_run_id;
    msg.trace.ts_ms = now;
    strncpy(msg.trace.channel, channel, sizeof(msg.trace.channel) - 1);
    msg.trace.setpoint = setpoint;
    msg.trace.sensor = sensor;
    msg.trace.value_real = real_out;
    msg.trace.value_shadow = shadow_out;
    msg.trace.stage = verify_get_stage();
    (void)xQueueSend(s_queue, &msg, 0);

    if (policy == SHADOW_STEP && s_armed) s_armed = false;
    return real_out;
}

float shadow_pid_preview(float setpoint, float sensor) {
    if (!isfinite(setpoint) || !isfinite(sensor)) return 0.0f;
    // Чистая функция от входов: replay даёт тот же результат.
    float out = 50.0f + 2.5f * (setpoint - sensor);
    if (out < 0.0f) out = 0.0f;
    if (out > 100.0f) out = 100.0f;
    return out;
}

void shadow_tick_heater(float real_out) {
    if (!verify_is_enabled() || verify_get_shadow_policy() == SHADOW_OFF) return;

    process_state_t *st = state_get();
    float setpoint = st->target_cube_t > 0.0f ? st->target_cube_t : 80.0f;
    shadow_compute("heater_power", setpoint, st->cube_temp, real_out,
                   shadow_pid_preview);
}

void shadow_log_adaptive(const char *channel, float setpoint, float sensor,
                         float real_out, float shadow_out,
                         float kp, float ki, float kd) {
    if (!verify_is_enabled() || !s_queue) return;
    if (verify_get_shadow_policy() == SHADOW_OFF) return;

    int64_t now = now_ms();
    if (!rate_allow(channel ? channel : "adaptive_pid", now)) return;
    if (s_run_id <= 0) s_run_id = shadow_start_run("adaptive");

    shadow_msg_t msg = {0};
    msg.trace.run_id = s_run_id;
    msg.trace.ts_ms = now;
    strncpy(msg.trace.channel, channel ? channel : "adaptive_pid",
            sizeof(msg.trace.channel) - 1);
    msg.trace.setpoint = setpoint;
    msg.trace.sensor = sensor;
    msg.trace.value_real = real_out;
    msg.trace.value_shadow = shadow_out;
    msg.trace.kp = kp;
    msg.trace.ki = ki;
    msg.trace.kd = kd;
    msg.trace.stage = verify_get_stage();
    (void)xQueueSend(s_queue, &msg, 0);
}

void shadow_request_run(void) { s_armed = true; }
void shadow_request_step(void) { s_armed = true; }
void shadow_cancel(void) { s_armed = false; }

int64_t shadow_start_run(const char *mode) {
    s_run_id = verify_db_is_ready()
        ? verify_db_start_run(mode, (int)verify_get_shadow_policy())
        : 0;
    if (s_run_id > 0) ESP_LOGI(TAG, "run %lld", (long long)s_run_id);
    return s_run_id;
}

esp_err_t shadow_finish_run(const char *metrics_json) {
    if (s_run_id <= 0) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_OK;
    if (metrics_json) err = verify_db_finish_run(s_run_id, metrics_json);
    s_run_id = 0;
    return err;
}

int64_t shadow_current_run(void) { return s_run_id; }
