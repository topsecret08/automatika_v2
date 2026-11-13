#include "pid_autotune.h"
#include "ctrl_pid.h"
#include "ctrl_safety.h"
#include "config.h"

#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "web/telegram_notify.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <math.h>

static const char *TAG = "AUTOTUNE";

static nvs_handle_t s_nvs;
static bool s_nvs_ok = false;
static volatile bool s_running = false;
static volatile bool s_done = false;
static int64_t s_started_us = 0;

esp_err_t pid_autotune_init(void) {
    s_nvs_ok = (nvs_open("pid", NVS_READWRITE, &s_nvs) == ESP_OK);

    float kp, ki, kd;
    if (pid_autotune_load_gains(&kp, &ki, &kd)) {
        process_state_t *st = state_get();
        st->pid_kp = kp;
        st->pid_ki = ki;
        st->pid_kd = kd;
        st->pid_tuned = true;

        if (control_config_get_mode() == CONTROL_MODE_PID) {
            ctrl_pid_set_tunings(kp, ki, kd);
        }
        ESP_LOGI(TAG, "loaded gains Kp=%.3f Ki=%.3f Kd=%.3f", kp, ki, kd);
    }
    return ESP_OK;
}

bool pid_autotune_load_gains(float *kp, float *ki, float *kd) {
    if (!s_nvs_ok) return false;

    float v[3];
    size_t len = sizeof(v);
    if (nvs_get_blob(s_nvs, "gains", v, &len) != ESP_OK || len != sizeof(v))
        return false;
    if (!isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2])) return false;
    if (v[0] <= 0.0f) return false;

    if (kp) *kp = v[0];
    if (ki) *ki = v[1];
    if (kd) *kd = v[2];
    return true;
}

static void save_gains(float kp, float ki, float kd) {
    if (!s_nvs_ok) return;
    float v[3] = { kp, ki, kd };
    nvs_set_blob(s_nvs, "gains", v, sizeof(v));
    nvs_commit(s_nvs);
}

esp_err_t pid_autotune_start(float target_temp) {
    if (pid_autotune_is_running()) return ESP_ERR_INVALID_STATE;
    if (estop_is_pressed() || !ctrl_safety_ok()) return ESP_ERR_INVALID_STATE;
    if (!isfinite(target_temp) || target_temp < 20.0f || target_temp > 120.0f)
        return ESP_ERR_INVALID_ARG;

    // В режиме BANG-BANG ctrl_pid_compute() не доходит до автотюнинга.
    if (control_config_get_mode() != CONTROL_MODE_PID) {
        ESP_LOGW(TAG, "autotune requires CONTROL_MODE_PID");
        return ESP_ERR_INVALID_STATE;
    }

    ctrl_pid_set_setpoint(target_temp);
    ctrl_pid_autotune_start();

    s_started_us = esp_timer_get_time();
    s_done = false;
    s_running = true;

    events_log(EVENT_INFO, "PID autotune: target %.1f °C", target_temp);
    telegram_notify_alert("PID", "Автотюнинг запущен");
    return ESP_OK;
}

void pid_autotune_stop(void) {
    if (!s_running) return;
    ctrl_pid_autotune_stop();
    s_running = false;
    events_log(EVENT_WARNING, "PID autotune cancelled");
}

bool pid_autotune_is_running(void) { return s_running; }
bool pid_autotune_done(void) { return s_done; }

void pid_autotune_task(void *arg) {
    (void)arg;
    wd_register("autotune", 10000, false);

    while (1) {
        WD_FEED("autotune");

        if (s_running) {
            if (ctrl_pid_autotune_done()) {
                float kp, ki, kd;
                ctrl_pid_autotune_get(&kp, &ki, &kd);

                if (isfinite(kp) && isfinite(ki) && isfinite(kd) && kp > 0.0f) {
                    save_gains(kp, ki, kd);

                    process_state_t *st = state_get();
                    st->pid_kp = kp;
                    st->pid_ki = ki;
                    st->pid_kd = kd;
                    st->pid_tuned = true;

                    s_done = true;
                    s_running = false;

                    char msg[96];
                    snprintf(msg, sizeof(msg),
                             "PID tuned: Kp=%.2f Ki=%.3f Kd=%.2f", kp, ki, kd);
                    events_log(EVENT_INFO, "%s", msg);
                    telegram_notify_alert("PID", msg);
                } else {
                    ESP_LOGW(TAG, "autotune finished with invalid gains");
                    s_running = false;
                }
            } else if (esp_timer_get_time() - s_started_us >
                       (int64_t)PID_AUTOTUNE_MAX_MS * 1000LL) {
                ESP_LOGW(TAG, "autotune timeout");
                ctrl_pid_autotune_stop();
                s_running = false;
                events_log(EVENT_WARNING, "PID autotune timeout");
            } else if (!ctrl_safety_ok() || estop_is_pressed()) {
                ESP_LOGW(TAG, "autotune aborted by safety");
                ctrl_pid_autotune_stop();
                s_running = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
