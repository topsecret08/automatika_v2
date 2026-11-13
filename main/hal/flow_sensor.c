#include "flow_sensor.h"
#include "config.h"
#include "hal_relay.h"

#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <math.h>

static const char *TAG = "FLOW";

static volatile uint32_t s_pulses = 0;
static volatile float s_lpm = 0.0f;
static bool s_present = true;
static bool s_initialized = false;
static nvs_handle_t s_nvs;
static bool s_nvs_ok = false;

// ------------------------------------------------------------------
static IRAM_ATTR void flow_isr(void *arg) {
    (void)arg;
    s_pulses++;
}

static void gpio_setup(void) {
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_FLOW_SENSOR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io);

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "gpio_install_isr_service: %s", esp_err_to_name(err));
    }
    err = gpio_isr_handler_add(PIN_FLOW_SENSOR, flow_isr, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ISR add failed: %s", esp_err_to_name(err));
    }
}

esp_err_t flow_init(void) {
#if !FLOW_SENSOR_ENABLED
    s_present = false;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_initialized) return ESP_OK;

    s_nvs_ok = (nvs_open("sensors", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (s_nvs_ok) {
        uint8_t v = 1;
        if (nvs_get_u8(s_nvs, "flow_present", &v) == ESP_OK) {
            s_present = (v != 0);
        }
    }

    gpio_setup();
    s_initialized = true;

    // Сброс счётчика и окно наблюдения.
    s_pulses = 0;

    if (s_present)
        ESP_LOGI(TAG, "YF-S201 on GPIO %d", PIN_FLOW_SENSOR);
    else
        ESP_LOGW(TAG, "flow sensor disabled in NVS");
    return ESP_OK;
#endif
}

void flow_set_present(bool present) {
    s_present = present;
    if (s_nvs_ok) {
        nvs_set_u8(s_nvs, "flow_present", present ? 1 : 0);
        nvs_commit(s_nvs);
    }
    events_log(EVENT_INFO, "Flow sensor %s", present ? "enabled" : "disabled");
}

bool flow_is_present(void) { return s_present; }

float flow_get_lpm(void) { return s_lpm; }

bool flow_is_ok(void) {
    if (!s_present) return true;           // нет датчика — нет блокировки
    return s_lpm >= FLOW_MIN_LPM;
}

// ------------------------------------------------------------------
void flow_task(void *arg) {
    (void)arg;
    wd_register("flow", FLOW_POLL_MS * 4, false);

    uint32_t last = s_pulses;
    int64_t last_us = esp_timer_get_time();
    float ema = 0.0f;
    bool alerted = false;

    while (1) {
        WD_FEED("flow");
        vTaskDelay(pdMS_TO_TICKS(FLOW_POLL_MS));

        uint32_t now_pulses = s_pulses;
        uint32_t delta = now_pulses - last;
        last = now_pulses;

        int64_t now_us = esp_timer_get_time();
        float dt = (float)(now_us - last_us) / 1e6f;
        last_us = now_us;
        if (dt <= 0.01f) dt = (float)FLOW_POLL_MS / 1000.0f;

        float lpm = ((float)delta * 60.0f) / (FLOW_PULSES_PER_LITER * dt);
        if (!isfinite(lpm) || lpm < 0.0f) lpm = 0.0f;

        // EMA сглаживает дребезг турбины, не задерживая реакцию дольше 2 с.
        ema = (ema <= 0.0f) ? lpm : (ema * 0.6f + lpm * 0.4f);
        s_lpm = ema;

        process_state_t *st = state_get();
        st->flow_lpm = ema;

        if (s_present && st->is_running && ema < FLOW_MIN_LPM) {
            if (!alerted) {
                alerted = true;
                events_log(EVENT_WARNING, "Flow low: %.2f L/min", ema);
            }
        } else {
            alerted = false;
        }
    }
}

// ------------------------------------------------------------------
void cooling_valve_set(bool open) {
    hal_relay_set(RELAY_WATER_VALVE, open);
}

bool cooling_valve_get(void) {
    return hal_relay_get(RELAY_WATER_VALVE);
}
