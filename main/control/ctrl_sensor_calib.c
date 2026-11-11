#include "ctrl_sensor_calib.h"
#include "hal/hal_1wire.h"
#include "core/events.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define MIN_SAMPLES      50
#define MAX_SAMPLES      200
#define SAMPLE_INTERVAL  500
#define STABILITY_TOL    0.15f
#define STABILITY_TIME   10000

static sensor_calib_t s;
static TaskHandle_t s_task = NULL;

void sensor_calib_init(void) {
    memset(&s, 0, sizeof(s));
    s.state = CAL_IDLE;
}

sensor_calib_t* sensor_calib_get(void) { return &s; }

void sensor_calib_start(float t_min, float t_max) {
    if (s.state == CAL_SAMPLING || s.state == CAL_HEATING) return;

    memset(&s, 0, sizeof(s));
    s.state = CAL_HEATING;
    s.start_ms = esp_timer_get_time() / 1000;
    s.target_temp_min = t_min;
    s.target_temp_max = t_max;
    s.use_min_max = (t_max > t_min);

    events_log(EVENT_INFO, "SensorCalib started: %.1f..%.1f°C", t_min, t_max);

    if (!s_task) {
        xTaskCreatePinnedToCore(sensor_calib_task, "senscal", 4096, NULL, 4,
                                &s_task, 1);
    }
}

void sensor_calib_abort(void) {
    s.state = CAL_IDLE;
    events_log(EVENT_WARNING, "SensorCalib aborted");
}

void sensor_calib_task(void *arg) {
    (void)arg;
    int n_sensors = hal_1wire_count();
    if (n_sensors < 2) {
        snprintf(s.error, sizeof(s.error), "Нужно ≥2 датчика");
        s.state = CAL_ERROR;
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    float prev_avg = 0;
    uint32_t stable_since = 0;

    // === Фаза 1: ожидание стабилизации ===
    while (s.state == CAL_HEATING) {
        float sum = 0;
        int v = 0;
        for (int i = 0; i < n_sensors; i++) {
            float t = hal_1wire_read(i);
            if (t > -100) { sum += t; v++; }
        }
        if (v == 0) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        float avg = sum / v;

        if (s.use_min_max &&
            (avg < s.target_temp_min || avg > s.target_temp_max)) {
            prev_avg = avg;
            stable_since = 0;
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL));
            continue;
        }

        if (fabsf(avg - prev_avg) < STABILITY_TOL) {
            if (stable_since == 0) stable_since = esp_timer_get_time() / 1000;
            uint32_t now = esp_timer_get_time() / 1000;
            if (now - stable_since > STABILITY_TIME) {
                s.state = CAL_SAMPLING;
                s.samples_count = 0;
                events_log(EVENT_INFO, "SensorCalib stable, sampling...");
            }
        } else {
            stable_since = 0;
        }
        prev_avg = avg;
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL));
    }

    // === Фаза 2: сбор данных ===
    while (s.state == CAL_SAMPLING) {
        for (int i = 0; i < n_sensors; i++) {
            float t = hal_1wire_read(i);
            if (t < -100) continue;
            s.sum[i] += t;
            s.sum2[i] += t * t;
            s.n[i]++;
        }
        s.samples_count++;

        if (s.samples_count >= MAX_SAMPLES) {
            s.state = CAL_COMPUTE;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL));
    }

    // === Фаза 3: вычисления ===
    if (s.state == CAL_COMPUTE) {
        for (int i = 0; i < n_sensors; i++) {
            if (s.n[i] == 0) { s.avg[i] = 0; s.std[i] = 0; continue; }
            s.avg[i] = s.sum[i] / s.n[i];
            float var = s.sum2[i] / s.n[i] - s.avg[i] * s.avg[i];
            s.std[i] = (var > 0) ? sqrtf(var) : 0;
        }

        float total = 0;
        int vn = 0;
        for (int i = 0; i < n_sensors; i++)
            if (s.n[i] > 0) { total += s.avg[i]; vn++; }
        s.reference_temp = (vn > 0) ? (total / vn) : 0;

        float sum_sq = 0;
        for (int i = 0; i < n_sensors; i++) {
            if (s.n[i] == 0) continue;
            float d = s.avg[i] - s.reference_temp;
            sum_sq += d * d;
        }
        s.std_deviation = (vn > 0) ? sqrtf(sum_sq / vn) : 0;

        for (int i = 0; i < n_sensors; i++) {
            if (s.n[i] == 0) { s.offset[i] = 0; continue; }
            s.offset[i] = s.reference_temp - s.avg[i];
        }

        s.state = CAL_DONE;
        events_log(EVENT_INFO,
            "SensorCalib done: ref=%.3f°C scatter=%.3f°C samples=%u",
            s.reference_temp, s.std_deviation, s.samples_count);
    }

    s_task = NULL;
    vTaskDelete(NULL);
}

void sensor_calib_set_manual_reference(float temp) {
    if (s.state != CAL_DONE) return;
    float delta = temp - s.reference_temp;
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) s.offset[i] += delta;
    s.reference_temp = temp;
    events_log(EVENT_INFO, "SensorCalib: manual ref=%.3f°C", temp);
}

bool sensor_calib_apply_and_save(void) {
    if (s.state != CAL_DONE) return false;
    int n = hal_1wire_count();
    for (int i = 0; i < n; i++) {
        const ds18b20_calib_t *old = hal_1wire_get_calib(i);
        float new_offset = (old ? old->offset : 0) + s.offset[i];
        hal_1wire_set_calib(i, new_offset, old ? old->scale : 1.0f);
    }
    hal_1wire_save_calib();
    s.saved = true;
    events_log(EVENT_INFO, "SensorCalib: applied & saved");
    return true;
}