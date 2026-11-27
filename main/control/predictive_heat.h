#pragma once
#include <stdbool.h>

typedef struct {
    float target_c;
    float current_c;
    float rate_c_min;
    float dt_s;
    float max_power_pct;
} pred_heat_in_t;

typedef struct {
    float predicted_c;
    float cap_pct;
    bool  active;
} pred_heat_out_t;

typedef struct {
    float k_heat;       // °C/min at 100% power (estimated)
    float k_loss;       // 1/min loss coefficient
    float ambient_c;
    float last_temp;
    float last_ms;
    bool  initialized;
} mpc_model_t;

void predictive_heat_init(void);
pred_heat_out_t predictive_heat_limit(const pred_heat_in_t *in);
// Full MPC: returns recommended power [0..max_power_pct].
float predictive_heat_mpc_power(const pred_heat_in_t *in, float ambient_c);
