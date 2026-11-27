#include "predictive_heat.h"
#include <math.h>
#include <stddef.h>

static mpc_model_t s_mpc = {
    .k_heat = 0.35f,    // °C/min at 100% power, conservative default
    .k_loss = 0.003f,   // 1/min
    .ambient_c = 25.0f,
    .last_temp = NAN,
    .last_ms = 0.0f,
    .initialized = false,
};

void predictive_heat_init(void) {
    s_mpc.k_heat = 0.35f;
    s_mpc.k_loss = 0.003f;
    s_mpc.ambient_c = 25.0f;
    s_mpc.last_temp = NAN;
    s_mpc.last_ms = 0.0f;
    s_mpc.initialized = false;
}

static void mpc_update_model(const pred_heat_in_t *in, float ambient_c) {
    if (!in || !isfinite(in->current_c)) return;
    if (isfinite(ambient_c) && ambient_c > -20.0f && ambient_c < 60.0f) {
        s_mpc.ambient_c = 0.95f * s_mpc.ambient_c + 0.05f * ambient_c;
    }
    // Estimate k_heat from observed rate when power is significant.
    // rate = k_heat*u/100 - k_loss*(T-Tamb)
    if (isfinite(in->rate_c_min) && in->max_power_pct > 5.0f) {
        float loss = s_mpc.k_loss * (in->current_c - s_mpc.ambient_c);
        float k_heat_obs = (in->rate_c_min + loss) * 100.0f / fmaxf(in->max_power_pct, 1.0f);
        if (k_heat_obs > 0.05f && k_heat_obs < 2.0f) {
            s_mpc.k_heat = 0.9f * s_mpc.k_heat + 0.1f * k_heat_obs;
        }
    }
    s_mpc.initialized = true;
}

static float mpc_simulate(float temp, float u_pct, float dt_min, int steps, float target, float ambient) {
    // Simulate forward with constant power; return cost.
    float cost = 0.0f;
    float t = temp;
    for (int i = 0; i < steps; i++) {
        float dT = s_mpc.k_heat * (u_pct / 100.0f) - s_mpc.k_loss * (t - ambient);
        t += dT * dt_min;
        float err = t - target;
        cost += err * err;
        if (t > target + 0.5f) {
            float ov = t - (target + 0.5f);
            cost += 8.0f * ov * ov; // overshoot penalty
        }
        if (t < target - 2.0f) {
            float under = (target - 2.0f) - t;
            cost += 0.5f * under * under;
        }
    }
    // Terminal penalty
    float terr = t - target;
    cost += 2.0f * terr * terr;
    return cost;
}

float predictive_heat_mpc_power(const pred_heat_in_t *in, float ambient_c) {
    if (!in || !isfinite(in->current_c) || !isfinite(in->target_c) || in->target_c <= 0.0f) {
        return in ? in->max_power_pct : 0.0f;
    }
    mpc_update_model(in, ambient_c);

    float max_p = fminf(fmaxf(in->max_power_pct, 0.0f), 100.0f);
    if (max_p <= 0.0f) return 0.0f;

    // If already at/above target, no heating.
    if (in->current_c >= in->target_c + 0.1f) return 0.0f;

    const int horizon = 12;        // steps
    const float dt_min = 0.25f;    // 15s per step = 3 min horizon
    float best_u = 0.0f;
    float best_cost = 1e30f;

    // Coarse-to-fine grid search (move-blocking MPC, constant u over horizon).
    for (int i = 0; i <= 20; i++) {
        float u = max_p * (float)i / 20.0f;
        float c = mpc_simulate(in->current_c, u, dt_min, horizon, in->target_c, s_mpc.ambient_c);
        // Penalize high power near target
        float approach = in->target_c - in->current_c;
        if (approach < 3.0f) c += 0.01f * u * u * (3.0f - approach);
        if (c < best_cost) { best_cost = c; best_u = u; }
    }
    // Refine around best
    float lo = fmaxf(0.0f, best_u - max_p * 0.05f);
    float hi = fminf(max_p, best_u + max_p * 0.05f);
    for (int i = 0; i <= 10; i++) {
        float u = lo + (hi - lo) * (float)i / 10.0f;
        float c = mpc_simulate(in->current_c, u, dt_min, horizon, in->target_c, s_mpc.ambient_c);
        float approach = in->target_c - in->current_c;
        if (approach < 3.0f) c += 0.01f * u * u * (3.0f - approach);
        if (c < best_cost) { best_cost = c; best_u = u; }
    }

    if (best_u < 0.0f) best_u = 0.0f;
    if (best_u > max_p) best_u = max_p;
    return best_u;
}

pred_heat_out_t predictive_heat_limit(const pred_heat_in_t *in) {
    pred_heat_out_t out = { .predicted_c = NAN, .cap_pct = 100.0f, .active = false };
    if (!in || !isfinite(in->current_c) || !isfinite(in->target_c) || !isfinite(in->rate_c_min)) {
        return out;
    }
    if (in->target_c <= 0.0f || in->max_power_pct <= 0.0f) return out;

    float ambient = s_mpc.ambient_c;
    float mpc_p = predictive_heat_mpc_power(in, ambient);

    // Predicted temperature at horizon with recommended power.
    float t = in->current_c;
    const int horizon = 12;
    const float dt_min = 0.25f;
    for (int i = 0; i < horizon; i++) {
        float dT = s_mpc.k_heat * (mpc_p / 100.0f) - s_mpc.k_loss * (t - ambient);
        t += dT * dt_min;
    }
    out.predicted_c = t;
    out.cap_pct = mpc_p;
    out.active = (mpc_p < in->max_power_pct - 0.5f) || (t > in->target_c + 0.2f);
    return out;
}
