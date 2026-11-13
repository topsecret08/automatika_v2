#include "estop.h"
#include "estop_logic.h"
#include "config.h"
#include "events.h"
#include "watchdog.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

static const char *TAG = "ESTOP";

static volatile bool s_pressed = false;   // защёлка (прерывание/опрос)
static volatile int64_t s_trigger_us = 0;
static bool     s_isr_installed = false;
static bool     s_ready = false;
static bool     s_force = false;
static char     s_reason[48] = "";

// ISR минимален: только защёлка. Она безопасна, потому что нагрев/реле
// снимаются не из ISR, а в safety-задаче (приоритет 15, ядро 1), которая
// опрашивает флаг каждые 100 мс.
static void IRAM_ATTR estop_isr(void *arg) {
    (void)arg;
    int64_t now = esp_timer_get_time();
    if (now - s_trigger_us < (int64_t)ESTOP_DEBOUNCE_MS * 1000) return;
    s_trigger_us = now;
    s_pressed = true;
}

static bool pin_down(void) {
    return estop_level_active(gpio_get_level((gpio_num_t)ESTOP_GPIO));
}

esp_err_t estop_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << ESTOP_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config: %s", esp_err_to_name(err));
        return err;
    }

    if (!s_isr_installed) {
        /* в проекте других ISR нет; 0 — отключить авто-установщик */
        esp_err_t isr = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
        if (isr != ESP_OK && isr != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "isr service: %s", esp_err_to_name(isr));
        } else {
            s_isr_installed = true;
        }
    }

    if (gpio_isr_handler_add((gpio_num_t)ESTOP_GPIO, estop_isr, NULL) != ESP_OK) {
        // без прерывания остаёмся на опросе в estop_task
        gpio_set_intr_type((gpio_num_t)ESTOP_GPIO, GPIO_INTR_DISABLE);
        ESP_LOGW(TAG, "ISR not attached, polling only");
    }

    s_ready = true;

    if (pin_down()) {
        s_pressed = true;
        events_log(EVENT_WARNING, "ESTOP: button down at boot");
    }
    ESP_LOGI(TAG, "ready on GPIO%d (NC, active level=%d, current=%d)",
             ESTOP_GPIO, ESTOP_ACTIVE_LEVEL,
             gpio_get_level((gpio_num_t)ESTOP_GPIO));
    return ESP_OK;
}

bool estop_button_down(void) {
    return s_ready && pin_down();
}

int estop_raw_level(void) {
    return s_ready ? gpio_get_level((gpio_num_t)ESTOP_GPIO) : -1;
}

bool estop_is_pressed(void) {
    if (!s_ready) return false;
    /* уровень линии проверяем на месте: потерянное прерывание или
       дребезг не должны позволить управлять нагревом */
    if (pin_down()) {
        if (!s_pressed) s_trigger_us = esp_timer_get_time();
        s_pressed = true;
    }
    return s_pressed || s_force;
}

bool estop_acknowledge(void) {
    if (!s_ready) return false;
    if (pin_down()) return false;      /* кнопка не отпущена */
    if (s_force) return false;         /* программная блокировка не снята */
    s_pressed = false;
    s_reason[0] = '\0';
    events_log(EVENT_INFO, "ESTOP: acknowledged");
    return true;
}

void estop_force(const char *reason) {
    s_force = true;
    snprintf(s_reason, sizeof(s_reason), "%s", reason ? reason : "force");
    events_log(EVENT_WARNING, "ESTOP forced: %s", s_reason);
}

const char *estop_source_name(void) {
    if (s_force) return s_reason[0] ? s_reason : "forced";
    if (s_pressed) return "E-STOP button";
    return "none";
}

uint32_t estop_since_ms(void) {
    return (uint32_t)(s_trigger_us / 1000);
}

void estop_task(void *arg) {
    (void)arg;
    wd_register("estop", 2000, true);

    bool last = false;
    while (1) {
        WD_FEED("estop");

        bool now = estop_is_pressed();
        if (now != last) {
            last = now;
            if (now) events_log(EVENT_EMERGENCY, "E-STOP ACTIVE");
        }

        /* опрос каждые 50 мс — не полагаемся только на прерывание */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
