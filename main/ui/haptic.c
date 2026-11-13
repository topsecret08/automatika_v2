#include "haptic.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#if HAPTIC_GPIO >= 0
static const char *TAG = "HAPTIC";
static esp_timer_handle_t s_off_timer = NULL;

static void haptic_off(void *arg) {
    (void)arg;
#if HAPTIC_ACTIVE_HIGH
    gpio_set_level((gpio_num_t)HAPTIC_GPIO, 0);
#else
    gpio_set_level((gpio_num_t)HAPTIC_GPIO, 1);
#endif
}
#endif


void haptic_init(void) {
#if HAPTIC_GPIO >= 0
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << HAPTIC_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    haptic_off(NULL);

    esp_timer_create_args_t args = {
        .callback = haptic_off,
        .arg = NULL,
        .name = "haptic_off",
    };
    if (esp_timer_create(&args, &s_off_timer) != ESP_OK) {
        ESP_LOGW(TAG, "timer create failed");
        s_off_timer = NULL;
    }
    ESP_LOGI(TAG, "haptic on GPIO%d", HAPTIC_GPIO);
#endif
}

void haptic_pulse(uint32_t ms) {
    if (ms == 0) return;
#if HAPTIC_GPIO >= 0
#if HAPTIC_ACTIVE_HIGH
    gpio_set_level((gpio_num_t)HAPTIC_GPIO, 1);
#else
    gpio_set_level((gpio_num_t)HAPTIC_GPIO, 0);
#endif
    if (s_off_timer) {
        esp_timer_stop(s_off_timer);
        esp_timer_start_once(s_off_timer, (uint64_t)ms * 1000ULL);
    }
#else
    (void)ms;
#endif
}

void haptic_short(void) { haptic_pulse(15); }
void haptic_error(void) { haptic_pulse(60); }
