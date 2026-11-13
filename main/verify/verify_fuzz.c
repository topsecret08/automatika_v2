#include "verify_fuzz.h"

#include <stdbool.h>
#include <math.h>

#define FUZZ_N 96

static float median(float *v, int n) {
    float tmp[FUZZ_N];
    for (int i = 0; i < n; i++) tmp[i] = v[i];
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (tmp[j] < tmp[i]) {
                float t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t;
            }
        }
    }
    return tmp[n / 2];
}

// Детектор: отклонение от медианы окна 9 больше 4 * MAD.
static bool is_anomaly(const float *x, int n, int idx) {
    if (idx < 4 || idx >= n - 4) return false;
    float w[9];
    for (int i = 0; i < 9; i++) w[i] = x[idx - 4 + i];
    float med = median(w, 9);
    float dev[9];
    for (int i = 0; i < 9; i++) dev[i] = fabsf(w[i] - med);
    float mad = median(dev, 9);
    if (mad < 0.05f) mad = 0.05f;
    return fabsf(x[idx] - med) > 4.0f * mad;
}

esp_err_t verify_fuzz_anomaly(verify_fuzz_result_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;

    float x[FUZZ_N];
    bool truth[FUZZ_N];

    for (int i = 0; i < FUZZ_N; i++) {
        // Детерминированный «нормальный» сигнал: медленный дрейф + шум.
        x[i] = 80.0f + 0.01f * (float)i +
               0.15f * sinf((float)i * 0.7f);
        truth[i] = false;
    }

    // Выбросы.
    x[20] = 120.0f; truth[20] = true;
    x[45] = 20.0f;  truth[45] = true;
    // Пропуск/обрыв.
    x[60] = -127.0f; truth[60] = true;
    // Дрейф-аномалия: несколько подряд.
    for (int i = 70; i < 75; i++) { x[i] += 4.0f; truth[i] = true; }

    out->tp = out->fp = out->fn = out->injected = 0;
    for (int i = 0; i < FUZZ_N; i++) {
        bool det = is_anomaly(x, FUZZ_N, i);
        if (truth[i]) {
            out->injected++;
            if (det) out->tp++;
            else out->fn++;
        } else if (det) {
            out->fp++;
        }
    }
    return (out->tp > 0) ? ESP_OK : ESP_FAIL;
}
