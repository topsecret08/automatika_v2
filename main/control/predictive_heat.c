#include "predictive_heat.h"
#include <math.h>

void predictive_heat_init(void) {}

pred_heat_out_t predictive_heat_limit(const pred_heat_in_t *in) {
    pred_heat_out_t out = { .predicted_c = NAN, .cap_pct = 100.0f, .active = false };
    if (!in || !isfinite(in->current_c) || !isfinite(in->target_c) || !isfinite(in->rate_c_min)) {
        return out;
    }
    if (in->target_c <= 0.0f || in->max_power_pct <= 0.0f) return out;

    // Simple predictive model: assume temp continues at current rate for a lookahead window,
    // with a small inertia factor. Cap power when overshoot is predicted.
    float lookahead_min = 3.0f; // 3 min horizon
    float inertia = 0.35f;      // fraction of current rate persisting after power cut
    float predicted = in->current_c + in->rate_c_min * lookahead_min * inertia;
    out.predicted_c = predicted;

    float overshoot = predicted - in->target_c;
    float cap = in->max_power_pct;
    if (overshoot > 0.5f) {
        // Linear cap: at overshoot >= 3C, cap to 0
        float k = (overshoot - 0.5f) / 2.5f;
        if (k > 1.0f) k = 1.0f;
        cap = in->max_power_pct * (1.0f - k);
        out.active = true;
    } else if (in->current_c > in->target_c + 0.2f) {
        cap = 0.0f;
        out.active = true;
    }
    if (cap < 0.0f) cap = 0.0f;
    if (cap > 100.0f) cap = 100.0f;
    out.cap_pct = cap;
    return out;
}
