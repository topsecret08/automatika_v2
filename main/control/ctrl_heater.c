#include "ctrl_heater.h"
#include "heater_logic.h"

#include "regulator_driver.h"
#include "verify/shadow.h"
#include "hal/hal_dac.h"
#include "hal/hal_1wire.h"
#include "control/ctrl_safety.h"
#include "core/control_config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "core/estop.h"
#include "level/level_sensor.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <math.h>

static volatile float s_pct = 0.0f;
static volatile float s_auto_pct = 0.0f;
static volatile float s_manual_pct = 0.0f;
static volatile bool  s_manual = false;
static volatile bool  s_trend_cut = false;
static SemaphoreHandle_t s_mtx = NULL;

static void lock(void) {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}
static void unlock(void) {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

static float clamp_power(float pct) {
    if (!isfinite(pct) || pct <= 0.0f) return 0.0f;
    if (pct >= 100.0f) return 100.0f;
    return pct;
}

static bool heater_allowed(float pct) {
    return heater_power_allowed(pct, estop_is_pressed(), ctrl_safety_ok(),
                                level_is_available(), level_is_too_low());
}

// Ограничение мощности при росте температуры.
static float trend_guard(float pct) {
    s_trend_cut = false;
    if (pct <= 0.0f) return 0.0f;
    if (hal_1wire_count() <= 0) return pct;

    if (hal_1wire_rising(0) && pct > TREND_GUARD_MIN_PCT) {
        float cut = pct * HEATER_TREND_CUT;
        if (cut < TREND_GUARD_MIN_PCT) cut = TREND_GUARD_MIN_PCT;
        if (cut < pct) {
            s_trend_cut = true;
            return cut;
        }
    }
    return pct;
}

// Применение к выбранному выходу. Вызывается под mutex.
static esp_err_t heater_apply(float pct) {
    pct = clamp_power(pct);

    if (!heater_allowed(pct))
        pct = 0.0f;

    pct = trend_guard(pct);
    s_pct = pct;
    state_get()->power_percent = pct;

    // Выбранный внешний регулятор нельзя незаметно заменять DAC.
    if (control_config_get_regulator() != REGULATOR_NONE) {
        hal_dac_set_percent(0);

        if (!regulator_is_ready()) {
            regulator_emergency_off();
            s_pct = 0.0f;
            state_get()->power_percent = 0.0f;
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err = regulator_set_power(pct);
        if (err != ESP_OK) {
            regulator_emergency_off();
            s_pct = 0.0f;
            state_get()->power_percent = 0.0f;
        }
        shadow_tick_heater(s_pct);
        return err;
    }

    hal_dac_set_percent(pct);
    shadow_tick_heater(pct);
    return ESP_OK;
}

void ctrl_heater_init(void) {
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();

    lock();
    s_pct = 0.0f;
    s_auto_pct = 0.0f;
    s_manual_pct = 0.0f;
    s_manual = false;

    hal_dac_init();
    hal_dac_set_percent(0);

    esp_err_t err = regulator_init_from_config();
    if (err != ESP_OK && control_config_get_regulator() != REGULATOR_NONE) {
        ESP_LOGE("HEATER", "regulator init failed: %s; output stays off",
                 esp_err_to_name(err));
        regulator_emergency_off();
        events_log(EVENT_WARNING, "Heater: regulator not ready (%s)",
                   esp_err_to_name(err));
    }
    unlock();
}

void ctrl_heater_set_percent(float pct) {
    pct = clamp_power(pct);

    lock();
    s_auto_pct = pct;
    // Режим продолжает вычисления, но не затирает ручную уставку.
    (void)heater_apply(heater_effective_pct(s_manual, s_manual_pct, pct));
    unlock();
}

esp_err_t ctrl_heater_manual_set(float pct) {
    if (!isfinite(pct)) return ESP_ERR_INVALID_ARG;
    pct = clamp_power(pct);

    if (!heater_allowed(pct))
        return ESP_ERR_INVALID_STATE;

    lock();
    s_manual_pct = pct;
    s_manual = true;
    esp_err_t err = heater_apply(pct);
    if (err != ESP_OK) {
        s_manual = false;
        s_manual_pct = 0.0f;
        (void)heater_apply(s_auto_pct);
    }
    unlock();
    return err;
}

void ctrl_heater_set_auto(void) {
    lock();
    s_manual = false;
    (void)heater_apply(s_auto_pct);
    unlock();
}

void ctrl_heater_force_off(void) {
    lock();
    s_manual = false;
    s_manual_pct = 0.0f;
    s_auto_pct = 0.0f;
    s_pct = 0.0f;
    state_get()->power_percent = 0.0f;

    hal_dac_set_percent(0);
    regulator_emergency_off();
    unlock();
}

bool ctrl_heater_manual_active(void) { return s_manual; }
float ctrl_heater_get_percent(void) { return s_pct; }
float ctrl_heater_get_auto_percent(void) { return s_auto_pct; }
bool ctrl_heater_trend_cut_active(void) { return s_trend_cut; }

bool ctrl_heater_service_set(float pct) {
    lock();
    esp_err_t err = heater_apply(clamp_power(pct));
    unlock();
    return err == ESP_OK;
}

void ctrl_heater_task(void *arg) {
    (void)arg;
    wd_register("heater", 3000, false);

    bool last_cut = false;
    while (1) {
        WD_FEED("heater");

        bool cut = s_trend_cut;
        if (cut != last_cut) {
            last_cut = cut;
            events_log(EVENT_INFO, "Heater: trend guard %s (slope %.1f C/min)",
                       cut ? "ON" : "off", hal_1wire_slope(0));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
