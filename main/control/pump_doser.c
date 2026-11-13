#include "pump_doser.h"
#include "config.h"

#include "control/ctrl_dose.h"
#include "core/control_config.h"

#include <math.h>

esp_err_t pump_calib_load(PumpCalib *c) {
    if (!c) return ESP_ERR_INVALID_ARG;

    uint32_t spm = control_config_get_steps_per_ml();
    if (spm == 0) spm = PUMP_STEPS_PER_ML_DEFAULT;

    c->steps_per_ml = spm;
    c->max_steps = (uint32_t)(DOSER_MAX_DOSE_ML * (float)spm);
    return ESP_OK;
}

esp_err_t pump_calib_save(const PumpCalib *c) {
    if (!c) return ESP_ERR_INVALID_ARG;
    // max_steps вычисляется из DOSER_MAX_DOSE_ML, в NVS хранится только
    // калибровка шагов на миллилитр.
    return control_config_set_steps_per_ml(c->steps_per_ml);
}

esp_err_t pump_dose_volume(float volume_ml, float rate_ml_per_min) {
    if (!isfinite(volume_ml) || volume_ml <= 0.0f) return ESP_ERR_INVALID_ARG;

    PumpCalib c;
    esp_err_t err = pump_calib_load(&c);
    if (err != ESP_OK) return err;

    double total = (double)volume_ml * (double)c.steps_per_ml;
    if (total > (double)c.max_steps) return ESP_ERR_INVALID_SIZE;

    float rate = rate_ml_per_min;
    if (!isfinite(rate) || rate <= 0.0f) rate = control_config_get_rate_ml_min();

    return ctrl_dose_volume(volume_ml, rate) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
