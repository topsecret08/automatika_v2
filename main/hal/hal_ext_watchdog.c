#include "hal_ext_watchdog.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "EXT_WDT";
static int s_pin = -1;

esp_err_t hal_ext_watchdog_init(int gpio_pin) {
    s_pin = -1;
    if (gpio_pin < 0) {
        ESP_LOGW(TAG, "external watchdog not installed (GPIO -1)");
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

    gpio_set_level(gpio_pin, 0);
    s_pin = gpio_pin;
    return ESP_OK;
}

void hal_ext_watchdog_kick(void) {
    if (s_pin < 0) return;
    /* Таймер ждёт перехода низкий → высокий. Низкий уровень держим короткое время. */
    gpio_set_level(s_pin, 0);
    esp_rom_delay_us(100);
    gpio_set_level(s_pin, 1);
}

bool hal_ext_watchdog_enabled(void) {
    return s_pin >= 0;
}
