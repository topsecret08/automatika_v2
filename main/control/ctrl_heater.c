#include "ctrl_heater.h"
#include "heater_logic.h"

#include "regulator_driver.h"
#include "verify/shadow.h"
#include "hal/hal_dac.h"
#include "hal/hal_lsa_pwm.h"
#include "hal/hal_1wire.h"
#include "hal/zmpt_rms.h"
#include "predictive_heat.h"
#include "energy_anomaly.h"
#include "control/ctrl_safety.h"
#include "core/control_config.h"
#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "core/hw_profile.h"
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
    bool process_or_service = state_get()->is_running ||
                              control_config_service_enabled();
    bool level_on = hw_has_device(HW_DEV_LEVEL);
    return heater_power_allowed(pct, estop_actions_blocked(), ctrl_safety_ok(),
                                level_on && level_is_available(),
                                level_on && level_is_too_low(),
                                level_on && level_is_required_fault(),
                                process_or_service);
}

// Ограничение мощности при росте температуры.
static float trend_guard(float pct) {
    s_trend_cut = false;
    if (pct <= 0.0f) return 0.0f;
    if (hal_1wire_count() <= 0) return pct;

    if (!hw_temp_available(HW_TEMP_CUBE)) return pct;
    if (hw_temp_rising(HW_TEMP_CUBE) && pct > TREND_GUARD_MIN_PCT) {
        float cut = pct * HEATER_TREND_CUT;
        if (cut < TREND_GUARD_MIN_PCT) cut = TREND_GUARD_MIN_PCT;
        if (cut < pct) {
            s_trend_cut = true;
            return cut;
        }
    }
    return pct;
}

static float s_power_v_nom = 220.0f;
static bool  s_zmpt_anomaly = false;
static energy_anomaly_t s_ea;
static bool s_ea_init = false;

static float power_compensate_pct(float pct) {
    if (pct <= 0.0f) return 0.0f;
    if (zmpt_rms_fault()) {
        if (!s_zmpt_anomaly) {
            s_zmpt_anomaly = true;
            events_log(EVENT_WARNING, "ZMPT: fault, power compensation disabled");
        }
        return pct;
    }
    float v = zmpt_rms_ctrl_v();
    if (!isfinite(v) || v < 150.0f || v > 260.0f) return pct;

    bool anomaly = (v < 195.0f || v > 245.0f);
    if (anomaly && !s_zmpt_anomaly) {
        s_zmpt_anomaly = true;
        events_log(EVENT_WARNING, "ZMPT: voltage anomaly %.1fV, compensating", v);
    } else if (!anomaly && s_zmpt_anomaly) {
        s_zmpt_anomaly = false;
        events_log(EVENT_INFO, "ZMPT: voltage back to normal %.1fV", v);
    }

    float ratio = s_power_v_nom / v;
    float compensated = pct * ratio * ratio;
    float max_boost = pct * 1.30f;
    if (compensated > max_boost) compensated = max_boost;
    if (compensated > 100.0f) compensated = 100.0f;
    if (compensated < 0.0f) compensated = 0.0f;
    return compensated;
}

// Применение мощности с компенсацией просадки/аномалий ZMPT. Вызывать под mutex.
static esp_err_t heater_apply(float pct) {
    pct = clamp_power(pct);

    if (!heater_allowed(pct))
        pct = 0.0f;

    pct = trend_guard(pct);
    // Модельно-предиктивное ограничение: не допускаем прогнозируемый перегрев колонны.
    if (hw_temp_available(HW_TEMP_COLUMN)) {
        float col = hw_temp_read(HW_TEMP_COLUMN);
        float tgt = state_get()->target_column_t;
        if (isfinite(col) && isfinite(tgt) && tgt > 0.0f) {
            pred_heat_in_t pin = {
                .target_c = tgt,
                .current_c = col,
                .rate_c_min = hw_temp_slope(HW_TEMP_COLUMN),
                .dt_s = 0.5f,
                .max_power_pct = pct,
            };
            pred_heat_out_t pout = predictive_heat_limit(&pin);
            if (pout.active && pout.cap_pct < pct) pct = pout.cap_pct;
        }
    }
    float apply_pct = power_compensate_pct(pct);
    s_pct = pct;
    state_get()->power_percent = pct;

    // Внешний регулятор получает логическую мощность; DAC/LSA — компенсированную.
    if (control_config_get_regulator() != REGULATOR_NONE) {
        hal_dac_set_percent(0);
        hal_lsa_pwm_force_off();

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

    hal_dac_set_percent(apply_pct);
    hal_lsa_pwm_set_duty_pct(apply_pct);
    shadow_tick_heater(apply_pct);
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
    hal_lsa_pwm_force_off();

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
    hal_lsa_pwm_force_off();
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
    energy_anomaly_init(&s_ea);
    s_ea_init = true;

    bool last_cut = false;
    bool last_ea = false;
    while (1) {
        WD_FEED("heater");

        bool cut = s_trend_cut;
        if (cut != last_cut) {
            last_cut = cut;
            events_log(EVENT_INFO, "Heater: trend guard %s (slope %.1f C/min)",
                       cut ? "ON" : "off",
                       hw_temp_slope(HW_TEMP_CUBE));
        }

        energy_sample_t es = {
            .v = zmpt_rms_ctrl_v(),
            .power_pct = s_pct,
            .temp_cube = hw_temp_read(HW_TEMP_CUBE),
            .dt_s = 0.5f,
        };
        energy_anomaly_tick(&s_ea, &es);
        bool ea = energy_anomaly_any(&s_ea);
        if (ea && !last_ea) {
            events_log(EVENT_WARNING, "Energy anomaly: %s", energy_anomaly_reason(&s_ea));
        }
        last_ea = ea;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
