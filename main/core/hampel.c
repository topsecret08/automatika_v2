#include "hampel.h"
#include <math.h>
#include <string.h>

static float med(const float *v, int n) {
    float t[HAMPEL_WINDOW];
    if (n <= 0) return NAN;
    if (n > HAMPEL_WINDOW) n = HAMPEL_WINDOW;
    memcpy(t, v, (size_t)n * sizeof(float));
    for (int i = 1; i < n; i++) {
        float k = t[i];
        int j = i - 1;
        while (j >= 0 && t[j] > k) { t[j + 1] = t[j]; j--; }
        t[j + 1] = k;
    }
    return (n & 1) ? t[n / 2] : 0.5f * (t[n / 2 - 1] + t[n / 2]);
}

void hampel_init(hampel_t *h, float iir_alpha) {
    if (!h) return;
    memset(h, 0, sizeof(*h));
    h->iir_alpha = (iir_alpha > 0.0f && iir_alpha <= 1.0f)
                       ? iir_alpha : 0.2f;
}

float hampel_process(hampel_t *h, float raw, bool *was_outlier) {
    if (!h || !isfinite(raw) || !was_outlier) return raw;

    h->buf[h->head] = raw;
    h->head = (h->head + 1) % HAMPEL_WINDOW;
    if (h->count < HAMPEL_WINDOW) h->count++;

    float m = med(h->buf, (int)h->count);
    float dev[HAMPEL_WINDOW] = {0};
    for (size_t i = 0; i < h->count; i++)
        dev[i] = fabsf(h->buf[i] - m);
    float mad = med(dev, (int)h->count);
    if (!isfinite(mad) || mad < 0.01f) mad = 0.01f;

    *was_outlier = fabsf(raw - m) > 3.0f * mad;
    if (*was_outlier) h->outlier_cnt++;

    float clean = *was_outlier ? m : raw;
    if (h->count == 1) h->iir_out = clean;
    else h->iir_out = (1.0f - h->iir_alpha) * h->iir_out +
                      h->iir_alpha * clean;
    return h->iir_out;
}
