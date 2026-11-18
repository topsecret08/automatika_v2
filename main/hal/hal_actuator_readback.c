#include "hal_actuator_readback.h"
#include "hal_adc.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>

static const char *TAG = "ACT_RB";

static rb_config_t g_cfg[16];
static size_t g_n;

void hal_actuator_readback_init(const rb_config_t *cfgs, size_t n) {
    memset(g_cfg, 0, sizeof(g_cfg));
    g_n = n > 16 ? 16 : n;
    if (cfgs && g_n) memcpy(g_cfg, cfgs, g_n * sizeof(rb_config_t));

    for (size_t i = 0; i < g_n; i++) {
        if (g_cfg[i].backend == RB_BACKEND_GPIO && g_cfg[i].pin >= 0) {
            gpio_config_t io = {
                .pin_bit_mask = 1ULL << g_cfg[i].pin,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            if (gpio_config(&io) != ESP_OK)
                ESP_LOGW(TAG, "GPIO %d config failed", g_cfg[i].pin);
        }
    }
}

void hal_actuator_readback_init_defaults(void) {
    rb_config_t cfg[ACT_RB_COUNT] = {
        [ACT_HEATER] = {
            .id = ACT_HEATER,
            .backend = (rb_backend_t)ACT_READBACK_BACKEND,
            .pin = ACT_READBACK_GPIO,
            .adc_channel = ACT_READBACK_ADC_CHANNEL,
            .full_scale = ACT_READBACK_FULL_SCALE,
        },
    };
    hal_actuator_readback_init(cfg, ACT_RB_COUNT);
}

float hal_actuator_readback(uint8_t id) {
    for (size_t i = 0; i < g_n; i++) {
        if (g_cfg[i].id != id) continue;
        switch (g_cfg[i].backend) {
        case RB_BACKEND_GPIO:
            if (g_cfg[i].pin < 0) return NAN;
            return hal_actuator_normalize_gpio(gpio_get_level(g_cfg[i].pin));
        case RB_BACKEND_CURRENT:
            return hal_actuator_normalize_current(
                (float)hal_adc_raw(g_cfg[i].adc_channel),
                g_cfg[i].full_scale);
        case RB_BACKEND_PWM_CAPTURE:
        case RB_BACKEND_NONE:
        default:
            return NAN;
        }
    }
    return NAN;
}

bool hal_actuator_has_feedback(uint8_t id) {
    for (size_t i = 0; i < g_n; i++)
        if (g_cfg[i].id == id && g_cfg[i].backend != RB_BACKEND_NONE)
            return true;
    return false;
}
