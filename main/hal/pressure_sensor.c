#include "pressure_sensor.h"
#include "baro.h"
#include "hal_adc.h"
#include "config.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>

static const char *TAG = "PRESS";

static float s_baseline_hpa = STANDARD_ATM_HPA;
static bool  s_baseline_set = false;
static bool  s_rezero_requested = true;
static int64_t s_last_zero_us = 0;

// Нуль шкалы нельзя обновлять, пока в системе есть избыточное давление:
// иначе аварийный порог «уедет» вместе с реальной аварией.
#define REZERO_MIN_INTERVAL_US (60LL * 1000000LL)
#define REZERO_MAX_ABS_KPA     5.0f

esp_err_t pressure_sensor_init(void) {
    if (baro_available()) {
        s_baseline_hpa = baro_get_pressure_hpa();
        s_baseline_set = true;
        s_last_zero_us = esp_timer_get_time();
        s_rezero_requested = false;
        ESP_LOGI(TAG, "baseline %.2f hPa (BMP280)", s_baseline_hpa);
    } else {
        ESP_LOGW(TAG, "no barometer, using analog 4-20 mA input");
    }
    return ESP_OK;
}

void pressure_sensor_rezero(void) {
    s_rezero_requested = true;
}

float column_pressure_kpa(void) {
    float kpa;

    if (baro_available()) {
        float p = baro_get_pressure_hpa();

        // Барометр мог определиться уже после pressure_sensor_init():
        // первый достоверный отсчёт всегда становится нулём шкалы.
        if (!s_baseline_set) {
            s_baseline_hpa = p;
            s_baseline_set = true;
            s_last_zero_us = esp_timer_get_time();
            s_rezero_requested = false;
        }

        if (s_rezero_requested) {
            float candidate = (p - s_baseline_hpa) * 0.1f;
            if (fabsf(candidate) <= REZERO_MAX_ABS_KPA &&
                esp_timer_get_time() - s_last_zero_us > REZERO_MIN_INTERVAL_US) {
                s_baseline_hpa = p;
                s_last_zero_us = esp_timer_get_time();
                s_rezero_requested = false;
            }
        }

        kpa = (p - s_baseline_hpa) * 0.1f;   // 1 гПа = 0.1 кПа
    } else {
        kpa = hal_adc_pressure_bar() * 100.0f;
    }

    if (!isfinite(kpa) || kpa < 0.0f) kpa = 0.0f;
    return kpa;
}

float column_pressure_bar(void) {
    return column_pressure_kpa() / 100.0f;
}

const char *pressure_sensor_source(void) {
    return baro_available() ? "bmp280" : "adc";
}
