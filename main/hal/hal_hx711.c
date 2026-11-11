#include "hal_hx711.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define HX711_TIMEOUT_US 200000

// КРИТИЧНО: mux должен быть статическим!
static portMUX_TYPE s_hx_mux = portMUX_INITIALIZER_UNLOCKED;

static inline void sck_hi(hx711_t *h) { gpio_set_level(h->pin_sck, 1); }
static inline void sck_lo(hx711_t *h) { gpio_set_level(h->pin_sck, 0); }

esp_err_t hx711_init(hx711_t *h, int dout, int sck, uint8_t gain) {
    memset(h, 0, sizeof(*h));
    h->pin_dout = dout;
    h->pin_sck  = sck;
    h->gain     = gain;
    h->scale    = 1.0f;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << dout) | (1ULL << sck),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_direction(dout, GPIO_MODE_INPUT);
    gpio_set_direction(sck,  GPIO_MODE_OUTPUT);
    sck_lo(h);

    int64_t start = esp_timer_get_time();
    while (gpio_get_level(dout) == 1) {
        if (esp_timer_get_time() - start > 1000000) return ESP_ERR_TIMEOUT;
        esp_rom_delay_us(100);
    }
    return ESP_OK;
}

bool hx711_is_ready(hx711_t *h) {
    return gpio_get_level(h->pin_dout) == 0;
}

int32_t hx711_read_raw(hx711_t *h) {
    int64_t start = esp_timer_get_time();
    while (!hx711_is_ready(h)) {
        if (esp_timer_get_time() - start > HX711_TIMEOUT_US) return INT32_MIN;
        esp_rom_delay_us(50);
    }

    uint32_t raw = 0;
    portENTER_CRITICAL(&s_hx_mux);

    for (int i = 0; i < 24; i++) {
        sck_hi(h);
        esp_rom_delay_us(1);
        raw = (raw << 1) | (gpio_get_level(h->pin_dout) ? 1 : 0);
        sck_lo(h);
        esp_rom_delay_us(1);
    }

    int extra = (h->gain == 32) ? 2 : (h->gain == 64) ? 3 : 1;
    for (int i = 0; i < extra; i++) {
        sck_hi(h); esp_rom_delay_us(1);
        sck_lo(h); esp_rom_delay_us(1);
    }

    portEXIT_CRITICAL(&s_hx_mux);

    if (raw & 0x800000) raw |= 0xFF000000;
    return (int32_t)raw;
}

int32_t hx711_read_avg(hx711_t *h, int n) {
    int64_t sum = 0;
    int valid = 0;
    for (int i = 0; i < n; i++) {
        int32_t v = hx711_read_raw(h);
        if (v == INT32_MIN) continue;
        sum += v;
        valid++;
    }
    return valid ? (int32_t)(sum / valid) : 0;
}

void hx711_tare(hx711_t *h, int samples) {
    h->offset = hx711_read_avg(h, samples);
    h->calibrated = true;
}

void hx711_set_scale(hx711_t *h, float scale) {
    h->scale = scale;
    h->calibrated = true;
}

float hx711_read_grams(hx711_t *h) {
    int32_t raw = hx711_read_avg(h, 8);
    return (raw - h->offset) / h->scale;
}

int32_t hx711_read_offset(hx711_t *h) { return h->offset; }