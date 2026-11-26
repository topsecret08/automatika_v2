#include "energy_anomaly.h"
#include <math.h>

void energy_anomaly_init(energy_anomaly_t *ea) {
    if (!ea) return;
    ea->avg_v = 220.0f;
    ea->avg_power = 0.0f;
    ea->last_temp = NAN;
    ea->temp_rate_ema = 0.0f;
    ea->voltage_unstable = false;
    ea->heater_ineffective = false;
    ea->power_oscillation = false;
    ea->ineffective_s = 0;
    ea->osc_count = 0;
    ea->last_power = 0.0f;
}

void energy_anomaly_tick(energy_anomaly_t *ea, const energy_sample_t *s) {
    if (!ea || !s || !isfinite(s->dt_s) || s->dt_s <= 0.0f) return;

    if (isfinite(s->v) && s->v > 100.0f && s->v < 300.0f) {
        ea->avg_v = ea->avg_v * 0.95f + s->v * 0.05f;
        ea->voltage_unstable = fabsf(s->v - ea->avg_v) > 15.0f;
    }
    if (isfinite(s->power_pct)) {
        float dp = fabsf(s->power_pct - ea->last_power);
        if (dp > 20.0f) ea->osc_count++;
        else if (ea->osc_count > 0) ea->osc_count--;
        ea->power_oscillation = ea->osc_count >= 5;
        ea->last_power = s->power_pct;
        ea->avg_power = ea->avg_power * 0.9f + s->power_pct * 0.1f;
    }

    if (isfinite(s->temp_cube)) {
        if (isfinite(ea->last_temp)) {
            float rate_c_min = (s->temp_cube - ea->last_temp) / s->dt_s * 60.0f;
            ea->temp_rate_ema = ea->temp_rate_ema * 0.8f + rate_c_min * 0.2f;
        }
        ea->last_temp = s->temp_cube;
    }

    // Heater commanded high, but temp barely rising for long time -> ineffective/suspect
    if (isfinite(s->power_pct) && s->power_pct > 60.0f && isfinite(ea->temp_rate_ema) && ea->temp_rate_ema < 0.05f) {
        ea->ineffective_s += (uint32_t)(s->dt_s);
    } else if (ea->ineffective_s > 0) {
        ea->ineffective_s = (ea->ineffective_s > 2) ? ea->ineffective_s - 2 : 0;
    }
    ea->heater_ineffective = ea->ineffective_s > 300; // 5 min
}

bool energy_anomaly_any(const energy_anomaly_t *ea) {
    return ea && (ea->voltage_unstable || ea->heater_ineffective || ea->power_oscillation);
}

const char *energy_anomaly_reason(const energy_anomaly_t *ea) {
    if (!ea) return "none";
    if (ea->heater_ineffective) return "heater_ineffective";
    if (ea->voltage_unstable) return "voltage_unstable";
    if (ea->power_oscillation) return "power_oscillation";
    return "none";
}
