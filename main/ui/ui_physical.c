#include "ui_physical.h"
#include "config.h"

#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "control/ctrl_heater.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "UI_PHYS";

static QueueHandle_t s_beep_q = NULL;
static bool s_alarm_led = false;
static bool s_heat_led = false;
static bool s_power_led = false;

static void led_write(int gpio, bool on) {
    if (gpio < 0) return;
#if LED_ACTIVE_HIGH
    gpio_set_level(gpio, on ? 1 : 0);
#else
    gpio_set_level(gpio, on ? 0 : 1);
#endif
}

static void led_init(int gpio) {
    if (gpio < 0) return;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    led_write(gpio, false);
}

void ui_physical_init(void) {
    led_init(LED_POWER_GPIO);
    led_init(LED_HEAT_GPIO);
    led_init(LED_ALARM_GPIO);

    if (BUZZER_GPIO >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << BUZZER_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        gpio_set_level(BUZZER_GPIO, 0);
    }

    s_beep_q = xQueueCreate(8, sizeof(int));
    if (!s_beep_q) ESP_LOGW(TAG, "beep queue unavailable");

    ui_physical_set_led_power(true);

    ESP_LOGI(TAG, "LED power=%d heat=%d alarm=%d buzzer=%d",
             LED_POWER_GPIO, LED_HEAT_GPIO, LED_ALARM_GPIO, BUZZER_GPIO);
}

void ui_physical_beep(int ms) {
    if (!s_beep_q || ms <= 0) return;
    int v = ms;
    // Не блокируем вызывающего: при переполнении сигнал просто пропускается.
    (void)xQueueSend(s_beep_q, &v, 0);
}

void ui_physical_set_led_power(bool on) {
    s_power_led = on;
    led_write(LED_POWER_GPIO, on);
}

void ui_physical_set_led_heat(bool on) {
    s_heat_led = on;
    led_write(LED_HEAT_GPIO, on);
}

void ui_physical_set_led_alarm(bool on) {
    s_alarm_led = on;
    led_write(LED_ALARM_GPIO, on);
}

void ui_physical_task(void *arg) {
    (void)arg;
    wd_register("ui_phys", 5000, false);

    bool prev_emergency = false;
    int blink = 0;

    while (1) {
        WD_FEED("ui_phys");

        // Buzzer: неблокирующая очередь.
        if (s_beep_q) {
            int ms;
            if (xQueueReceive(s_beep_q, &ms, 0) == pdTRUE) {
                if (BUZZER_GPIO >= 0) {
                    gpio_set_level(BUZZER_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(ms));
                    gpio_set_level(BUZZER_GPIO, 0);
                }
            }
        }

        process_state_t *st = state_get();

        if (!s_power_led) ui_physical_set_led_power(true);

        bool heating = ctrl_heater_get_percent() > 0.5f;
        if (heating != s_heat_led) ui_physical_set_led_heat(heating);

        if (st->emergency) {
            if (!prev_emergency) {
                prev_emergency = true;
                ui_physical_beep(800);
                events_log(EVENT_WARNING, "Physical UI: alarm indication");
            }
            // Мигание аварийного LED раз в 250 мс.
            blink = (blink + 1) % 10;
            led_write(LED_ALARM_GPIO, (blink < 5));
        } else {
            if (prev_emergency) {
                prev_emergency = false;
                ui_physical_set_led_alarm(false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
