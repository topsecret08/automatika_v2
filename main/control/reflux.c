#include "reflux.h"
#include "config.h"
#include "ctrl_valves.h"

#include "core/events.h"
#include "core/watchdog.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>

static const char *TAG = "REFLUX";

static volatile float s_ratio = REFLUX_RATIO_DEFAULT;
static volatile bool  s_enabled = false;
static int64_t s_cycle_start = 0;
static bool s_valve_open = false;

esp_err_t reflux_init(float ratio) {
    if (!isfinite(ratio) || ratio < 0.0f) ratio = 0.0f;
    if (ratio > REFLUX_RATIO_MAX) ratio = REFLUX_RATIO_MAX;

    s_ratio = ratio;
    s_enabled = false;
    s_cycle_start = esp_timer_get_time();
    s_valve_open = false;

    ctrl_valve_set_pct(REFLUX_VALVE_CHANNEL, 0.0f);
    ESP_LOGI(TAG, "init: ratio %.2f, valve closed", ratio);
    return ESP_OK;
}

void reflux_set_ratio(float ratio) {
    if (!isfinite(ratio)) return;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > REFLUX_RATIO_MAX) ratio = REFLUX_RATIO_MAX;

    if (fabsf(ratio - s_ratio) > 0.01f) {
        s_ratio = ratio;
        events_log(EVENT_INFO, "Reflux ratio %.2f", ratio);
    }
}

float reflux_get_ratio(void) { return s_ratio; }

void reflux_enable(bool on) {
    if (s_enabled == on) return;
    s_enabled = on;
    s_cycle_start = esp_timer_get_time();
    ESP_LOGI(TAG, "reflux %s", on ? "enabled" : "disabled");
}

bool reflux_enabled(void) { return s_enabled; }

void reflux_update(void) {
    if (!s_enabled) {
        if (s_valve_open) {
            ctrl_valve_set_pct(REFLUX_VALVE_CHANNEL, 0.0f);
            s_valve_open = false;
        }
        return;
    }

    int64_t now = esp_timer_get_time();
    if (s_cycle_start == 0) s_cycle_start = now;

    int64_t elapsed = now - s_cycle_start;
    int64_t period = REFLUX_PERIOD_US;
    if (elapsed >= period) {
        s_cycle_start += (elapsed / period) * period;
        elapsed %= period;
    }

    float ratio = s_ratio;
    int64_t open_us = (int64_t)((1.0f / (1.0f + ratio)) * (float)period);
    if (open_us < 0) open_us = 0;
    if (open_us > period) open_us = period;

    bool open = elapsed < open_us;
    if (open != s_valve_open) {
        s_valve_open = open;
        ctrl_valve_set_pct(REFLUX_VALVE_CHANNEL, open ? 100.0f : 0.0f);
    }
}

void reflux_task(void *arg) {
    (void)arg;
    wd_register("reflux", 3000, false);

    while (1) {
        WD_FEED("reflux");
        reflux_update();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
