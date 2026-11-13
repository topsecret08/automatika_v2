#include "pid_metrics.h"

#include <stdbool.h>
#include <math.h>

esp_err_t pid_compute_metrics(const float *sp, const float *y, int n,
                              float dt, float step_value, pid_metrics_t *out) {
    if (!sp || !y || !out || n < 2 || dt <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    float y_max = y[0];
    float amp = (step_value > 0.0f) ? step_value : fabsf(sp[n - 1] - sp[0]);
    for (int i = 1; i < n; i++) {
        if (y[i] > y_max) y_max = y[i];
    }

    out->overshoot_pct = (amp > 0.0f)
        ? fmaxf(0.0f, (y_max - amp) / amp * 100.0f)
        : 0.0f;

    // Settling: последний выход за 5 % от амплитуды.
    out->settling_s = 0.0f;
    for (int i = n - 1; i >= 0; i--) {
        if (fabsf(y[i] - sp[i]) > 0.05f * amp) {
            out->settling_s = (float)i * dt;
            break;
        }
    }

    // SSE: среднее отклонение на последних 20 %.
    int k0 = (int)(0.8f * (float)n);
    if (k0 < 0) k0 = 0;
    float sum = 0.0f;
    int cnt = 0;
    for (int i = k0; i < n; i++) {
        sum += fabsf(y[i] - sp[i]);
        cnt++;
    }
    out->sse = cnt ? sum / (float)cnt : 0.0f;

    // Oscillation index: доля смен знака производной.
    int changes = 0;
    int prev_sign = 0;
    for (int i = 1; i < n; i++) {
        int sg = (y[i] > y[i - 1]) ? 1 : ((y[i] < y[i - 1]) ? -1 : 0);
        if (sg && prev_sign && sg != prev_sign) changes++;
        if (sg) prev_sign = sg;
    }
    out->osc_index = (float)changes / (float)(n - 1);

    bool ok = out->overshoot_pct < PID_OVERSHOOT_MAX &&
              out->settling_s < PID_SETTLING_MAX &&
              out->sse < PID_SSE_MAX &&
              out->osc_index < PID_OSC_MAX;
    return ok ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
