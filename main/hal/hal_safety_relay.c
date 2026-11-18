#include "hal_safety_relay.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "S_RELAY";

static int g_pin = -1;
static uint32_t g_timeout_ms = 1000;
static volatile int64_t g_last_hb_us;
static volatile bool g_forced_open;
static bool g_ready;
static TimerHandle_t g_timer;

static void relay_timer_cb(TimerHandle_t t) {
    (void)t;
    bool closed = g_ready &&
                  safety_relay_should_close(esp_timer_get_time(),
                                            g_last_hb_us, g_timeout_ms,
                                            g_forced_open);
    gpio_set_level(g_pin, closed ? 1 : 0);
}

esp_err_t hal_safety_relay_init(int gpio_pin, uint32_t timeout_ms) {
    g_pin = gpio_pin;
    g_timeout_ms = timeout_ms > 0 ? timeout_ms : 1000;
    g_last_hb_us = 0;
    g_forced_open = false;
    g_ready = false;

    if (gpio_pin < 0) {
        ESP_LOGW(TAG, "safety relay disabled (GPIO -1)");
        return ESP_OK;
    }

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    gpio_set_level(gpio_pin, 0);  // fail-safe: старт разомкнут
    g_ready = true;

    uint32_t period = g_timeout_ms / 4;
    if (period < 50) period = 50;
    g_timer = xTimerCreate("srelay", pdMS_TO_TICKS(period), pdTRUE,
                           NULL, relay_timer_cb);
    if (!g_timer) {
        g_ready = false;
        return ESP_ERR_NO_MEM;
    }
    xTimerStart(g_timer, 0);
    return ESP_OK;
}

void hal_safety_relay_heartbeat(void) {
    if (!g_ready || g_forced_open) return;
    g_last_hb_us = esp_timer_get_time();
}

bool hal_safety_relay_is_closed(void) {
    return g_ready &&
           safety_relay_should_close(esp_timer_get_time(), g_last_hb_us,
                                     g_timeout_ms, g_forced_open);
}

void hal_safety_relay_force_open(void) {
    g_forced_open = true;
    if (g_pin >= 0) gpio_set_level(g_pin, 0);
}
