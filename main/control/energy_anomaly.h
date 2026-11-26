#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float v;
    float power_pct;
    float temp_cube;
    float dt_s;
} energy_sample_t;

typedef struct {
    float avg_v;
    float avg_power;
    float last_temp;
    float temp_rate_ema;
    bool  voltage_unstable;
    bool  heater_ineffective;
    bool  power_oscillation;
    uint32_t ineffective_s;
    uint32_t osc_count;
    float last_power;
} energy_anomaly_t;

void energy_anomaly_init(energy_anomaly_t *ea);
void energy_anomaly_tick(energy_anomaly_t *ea, const energy_sample_t *s);
bool energy_anomaly_any(const energy_anomaly_t *ea);
const char *energy_anomaly_reason(const energy_anomaly_t *ea);
