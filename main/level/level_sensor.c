#include "level_sensor.h"
#include "config.h"

#include "core/events.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>

static const char *TAG = "LEVEL";

static bool  s_ready = false;
static bool  s_available = false;
static bool  s_fault = false;
static float s_percent = NAN;          /* с гистерезисом */
static float s_percent_raw = NAN;
static int64_t s_last_poll_us = 0;

static const char *KIND_NAME[] = {"none", "float", "ultrasonic"};

const char *level_state_name(void) {
#if LEVEL_SENSOR_ENABLED == 0
    return "disabled";
#else
    return KIND_NAME[LEVEL_KIND_FLOAT];
#endif
}

// ------------------------------------------------------------------
// Поплавковый датчик: один GPIO, INPUT_PULLUP.
// ------------------------------------------------------------------
static bool float_water_present(void) {
    int lvl = gpio_get_level((gpio_num_t)LEVEL_SENSOR_GPIO);
#if LEVEL_SENSOR_ACTIVE_LOW
    return lvl == 0;      /* замкнут на GND = вода есть */
#else
    return lvl == 1;      /* замкнут на VCC = вода есть */
#endif
}

esp_err_t level_sensor_init(void) {
#if LEVEL_SENSOR_ENABLED == 0
    ESP_LOGW(TAG, "level sensor disabled in config.h");
    return ESP_OK;
#else
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << LEVEL_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = LEVEL_SENSOR_ACTIVE_LOW ? GPIO_PULLUP_ENABLE
                                              : GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_ready = true;
    s_available = true;
    level_sensor_poll();

    events_log(EVENT_INFO, "Level sensor ready (%s on GPIO%d)",
               level_state_name(), LEVEL_SENSOR_GPIO);
    ESP_LOGI(TAG, "level sensor %s on GPIO%d, initial state=%s",
             level_state_name(), LEVEL_SENSOR_GPIO, level_state_name());
    return ESP_OK;
#endif
}

void level_sensor_poll(void) {
    if (!s_ready) return;
    s_last_poll_us = esp_timer_get_time();

    bool water = float_water_present();

    s_percent_raw = water ? 100.0f : 0.0f;

    /* гистерезис: не меняем «здоровое» состояние из-за дребезга поплавка */
    if (!isfinite(s_percent)) {
        s_percent = s_percent_raw;
    } else if (fabsf(s_percent_raw - s_percent) >= LEVEL_HYST_PCT) {
        s_percent = s_percent_raw;
    }

    bool fault_now = false;
    if (water && !s_available) {
        /* появление воды после «пусто» — датчик ожил */
        s_available = true;
        fault_now = false;
    }
    if (!water && !s_available) fault_now = true;

    if (fault_now != s_fault) {
        s_fault = fault_now;
        events_log(fault_now ? EVENT_WARNING : EVENT_INFO,
                   "Level sensor %s", fault_now ? "no signal" : "ok");
    }
}

float level_get_percent(void) { return s_percent; }
float level_get_raw_percent(void) { return s_percent_raw; }

bool level_is_available(void) {
#if LEVEL_SENSOR_ENABLED == 0
    return false;
#else
    return s_ready;
#endif
}

bool level_is_required_fault(void) {
#if (LEVEL_SENSOR_ENABLED == 0) || (LEVEL_SENSOR_REQUIRED == 0)
    return false;
#else
    return !s_ready;
#endif
}

bool level_is_fault(void) { return s_fault; }

/* Поплавковый датчик различает только «есть вода / нет воды», поэтому
 * «слишком низко» == нет воды. Переполнение для поплавка не
 * определяется; при установке ультразвука это станет сравнением с
 * LEVEL_MAX_PERCENT. */
bool level_is_too_low(void) {
    if (!level_is_available()) return false;    /* нет датчика — не блокируем */
    if (!isfinite(s_percent)) return false;
    return s_percent <= (100.0f - LEVEL_MAX_PERCENT);   /* меньше ~5 % */
}

bool level_is_overflow(void) {
    if (!level_is_available()) return false;
    if (!isfinite(s_percent)) return false;
    /* для поплавка: если вода есть и при этом сработал второй (верхний)
     * контакт — не поддерживается этим типом датчика */
    return s_percent > LEVEL_MAX_PERCENT && s_percent_raw > 100.0f;
}
