#include "ctrl_phlegm_pump.h"
#include "config.h"

#include "core/estop.h"
#include "core/events.h"
#include "control/ctrl_safety.h"
#include "hal/hal_adc.h"
#include "hal/hal_mcp23017.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>

static const char *TAG = "PHLEGM";

static bool s_ready = false;
static bool s_pump_on = false;
static bool s_level_high = false;
static float s_last_v = NAN;
static int64_t s_last_change_us = 0;

static float read_float_voltage(void) {
#if PHLEGM_FLOAT_ANALOG_ENABLED
    int16_t raw = 0;
    if (PHLEGM_FLOAT_ADC_DEV == 2) {
        raw = hal_adc2_raw(PHLEGM_FLOAT_ADC_CH);
    } else {
        raw = hal_adc_raw(PHLEGM_FLOAT_ADC_CH);
    }
    // ADS1115: raw / 32768 * 6.144V (PGA 6.144)
    return ((float)raw / 32768.0f) * 6.144f;
#else
    return NAN;
#endif
}

static bool read_level_high(void) {
#if PHLEGM_FLOAT_ANALOG_ENABLED
    float v = read_float_voltage();
    s_last_v = v;
    if (!isfinite(v)) return s_level_high;
    // Поплавок вверх -> контакт на GND -> напряжение падает.
    if (s_level_high) {
        // Уже высокий уровень: сбрасываем только когда напряжение явно поднялось.
        if (v >= PHLEGM_FLOAT_V_OFF) return false;
        return true;
    } else {
        if (v <= PHLEGM_FLOAT_V_ON) return true;
        return false;
    }
#else
    // Цифровой режим: MCP23017 GPB2, pull-up, active-low (замыкание на GND = высокий уровень)
    if (!hal_mcp23017_is_ready()) return false;
    bool pin = hal_mcp23017_read_pin(LEVEL_PHLEGM_REED_MCP_PIN);
    s_last_v = NAN;
    return !pin; // active-low
#endif
}

static void pump_set(bool on) {
    if (s_pump_on == on) return;
    if (!hal_mcp23017_is_ready()) return;
    hal_mcp23017_write_pin(PHLEGM_PUMP_SSR_CH, on);
    s_pump_on = on;
    s_last_change_us = esp_timer_get_time();
    events_log(EVENT_INFO, "Phlegm pump %s (level=%s, v=%.2f)",
               on ? "ON" : "OFF",
               s_level_high ? "HIGH" : "OK",
               isfinite(s_last_v) ? s_last_v : -1.0f);
    ESP_LOGI(TAG, "pump %s level_high=%d v=%.3f", on ? "ON" : "OFF",
             s_level_high, isfinite(s_last_v) ? s_last_v : -1.0f);
}

esp_err_t ctrl_phlegm_pump_init(void) {
#if !PHLEGM_PUMP_ENABLED
    s_ready = false;
    return ESP_OK;
#else
    s_ready = true;
    s_pump_on = false;
    s_level_high = false;
    s_last_v = NAN;
    s_last_change_us = 0;
    if (hal_mcp23017_is_ready()) {
        hal_mcp23017_write_pin(PHLEGM_PUMP_SSR_CH, false);
    }
    ESP_LOGI(TAG, "phlegm pump init (analog=%d dev=%d ch=%d on<=%.2fV off>=%.2fV)",
             (int)PHLEGM_FLOAT_ANALOG_ENABLED, (int)PHLEGM_FLOAT_ADC_DEV,
             (int)PHLEGM_FLOAT_ADC_CH, (double)PHLEGM_FLOAT_V_ON, (double)PHLEGM_FLOAT_V_OFF);
    return ESP_OK;
#endif
}

void ctrl_phlegm_pump_force_off(void) {
    if (!s_ready) return;
    if (hal_mcp23017_is_ready()) {
        hal_mcp23017_write_pin(PHLEGM_PUMP_SSR_CH, false);
    }
    if (s_pump_on) {
        s_pump_on = false;
        s_last_change_us = esp_timer_get_time();
    }
}

void ctrl_phlegm_pump_poll(void) {
#if !PHLEGM_PUMP_ENABLED
    return;
#else
    if (!s_ready) return;

    // Безопасность: при E-STOP/bench/blocked — насос OFF.
    if (estop_actions_blocked() || !ctrl_safety_ok()) {
        ctrl_phlegm_pump_force_off();
        // всё равно обновим состояние уровня для диагностики
        s_level_high = read_level_high();
        return;
    }

    s_level_high = read_level_high();

    int64_t now = esp_timer_get_time();
    int64_t elapsed_ms = s_last_change_us ? (now - s_last_change_us) / 1000 : 1000000;

    if (s_level_high) {
        if (!s_pump_on && elapsed_ms >= (int64_t)PHLEGM_PUMP_MIN_OFF_MS) {
            pump_set(true);
        }
    } else {
        if (s_pump_on && elapsed_ms >= (int64_t)PHLEGM_PUMP_MIN_ON_MS) {
            pump_set(false);
        }
    }
#endif
}

bool ctrl_phlegm_pump_is_on(void) { return s_pump_on; }
bool ctrl_phlegm_pump_level_high(void) { return s_level_high; }
float ctrl_phlegm_pump_float_voltage(void) { return s_last_v; }
